#include <stdint.h>
#include <stdarg.h>
#include "../boot_info.h"

#ifndef KERNEL_LOAD_ADDRESS
    #error KERNEL_LOAD_ADDRESS not defined
#endif
#ifndef KERNEL_FIRST_SECTOR_LBA
    #error KERNEL_FIRST_SECTOR_LBA not defined
#endif
#ifndef KERNEL_SECTORS
    #error KERNEL_SECTORS not defined
#endif


boot_info_t boot_info; // to pass to the kernel
char global_buffer[128]; // for printf() and others
const char *hex_digits = "0123456789abcdef";
const char *menu_choices = "123456789abcdefghijklmnopqrstuvwxyz";
uint16_t supported_vbe_modes[30];
uint16_t supported_vbe_modes_count = 0;
uint16_t selected_graphics_mode;
int serial_port_initialized = 0;
uint8_t _reg8_;
uint16_t _reg16_;
uint32_t _reg32_;

// ----------------------------------------------------------

extern uint32_t asm_return_bp2_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp4_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp6_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp8_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp10_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp12_arg(uint32_t word1, uint32_t word2);
extern uint8_t vbe_get_ctrl_info_real(void *ptr);
extern uint8_t vbe_get_mode_info_real(uint16_t mode, void *ptr);
extern uint8_t vbe_set_mode_real(void);
extern uint8_t bios_read_sectors_asm(uint32_t dap_ptr);
extern void enter_protected_mode(); // make sure arg is uint32_t, a long pointer

// -------------------------------------------------------

static inline void halt() {
    for(;;) asm("hlt");
}
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// -------------------------------------------------------

static void initialize_serial_port() {
    outb(0x3F8 + 1, 0x00); // disable interrupts
    outb(0x3F8 + 3, 0x80); // enable DLAB
    outb(0x3F8 + 0, 0x01); // baud divisor low  (115200 / 1 = 115200)
    outb(0x3F8 + 1, 0x00); // baud divisor high
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7); // enable FIFO
    outb(0x3F8 + 4, 0x0B); // IRQs disabled, RTS/DSR set

    serial_port_initialized = 1;
}
static void serial_print_char(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0); // Wait for transmit buffer empty
    outb(0x3F8, c);
}

// -------------------------------------------------------

typedef struct menu_item {
    char text[64];
    uint32_t data;
} menu_item;
typedef struct menu {
    char *title[64];
    int items_count;
    menu_item menu_items[20]; // 1-9, a-k
} menu;

// -------------------------------------------------------

int strlen(const char* str) {
	int len = 0;
	while (str[len]) len++;
	return len;
}
void strcpy(char *target, const char *source) {
    while (*source != '\0') *target++ = *source++;
    *target = *source; // final null char
}
void vsnprintf(char *buffer, int size, const char *fmt, va_list vl) {
    char c, digits[32], pad_char;
    int pad_width, start, negative;

    while (*fmt != 0 && size > 1) {
        c = *fmt++;
        if (c != '%') {
            *buffer++ = c;
            size--;
            continue;
        }
        // so we are in a '%'. parse justification, zeros, width
        pad_char = ' ';
        pad_width = 0;
        negative = 0;
        c = *fmt++;
        if (c == '0') { pad_char = '0'; c = *fmt++; }
        while (c >= '0' && c <= '9') { pad_width = (pad_width * 10) + (c - '0'); c = *fmt++; }

        // now apply modifier
        if (c == 'd') {
            int d = va_arg(vl, int);
            start = sizeof(digits) - 1;
            digits[start] = 0;
            if (d == 0 && size > 1) {
                digits[--start] = '0';
            } else {
                if (d < 0) { negative = 1; d = -d; }
                while (d > 0) { digits[--start] = '0' + (d % 10); d /= 10; }
                if (negative) digits[--start] = '-';
            }
            pad_width -= (sizeof(digits) - 1 - start);
            while (pad_width-- > 0 && size > 1) { *buffer++ = pad_char; size--; }
            while (digits[start] != 0 && size > 1) { *buffer++ = digits[start++]; size--; }
            
        } else if (c == 'x') {
            unsigned int u = va_arg(vl, int);
            start = sizeof(digits) - 1;
            digits[start] = 0;
            while (u > 0) { digits[--start] = hex_digits[u & 0xF]; u >>= 4; }
            pad_width -= (sizeof(digits) - 1 - start);
            while (pad_width-- > 0 && size > 1) { *buffer++ = pad_char; size--; }
            while (digits[start] != 0 && size > 1) { *buffer++ = digits[start++]; size--; }

        } else if (c == 'c') {
            char chr = (char)va_arg(vl, int);
            if (size > 1) { *buffer++ = chr; size--; }
        } else if (c == 's') {
            char *str = va_arg(vl, char *);
            while (*str != 0 && size > 1) { *buffer++ = *str++; size--; pad_width--; }
            while (pad_width-- > 0 && size > 1) { *buffer++ = pad_char; size--; }
        } else {
            if (size > 1) { *buffer++ = c; size--; }
        }
    }

    *buffer++ = 0;
    size--;
}

// -------------------------------------------------------

static void bios_print_char(char c) {
    // bios teletype int 0x10
    asm volatile ("movb $0x0E, %%ah\n" "movb %0, %%al\n"  "int $0x10\n" : : "m"(c) : "ax");

    // for debugging (copy/paste) in QEMU
    if (serial_port_initialized)
        serial_print_char(c);
}
static void bios_print_str(char *s) {
    while (*s) { bios_print_char(*s); s++; }
}
static void bios_print_int(int value) {
    if (value == 0) { bios_print_char('0'); return; }
    char buff[16];
    int negative = (value < 0);
    if (negative) value = -value;
    int idx = sizeof(buff) - 1;
    buff[idx] = 0;
    while (value > 0) {
        buff[--idx] = '0' + (value % 10);
        value /= 10;
    }
    if (negative) buff[--idx] = '-';
    bios_print_str(buff + idx);
}
static void bios_print_hex32(uint32_t value) {
    bios_print_char(hex_digits[(value >> 28) & 0xF]);
    bios_print_char(hex_digits[(value >> 24) & 0xF]);
    bios_print_char(hex_digits[(value >> 20) & 0xF]);
    bios_print_char(hex_digits[(value >> 16) & 0xF]);
    bios_print_char(hex_digits[(value >> 12) & 0xF]);
    bios_print_char(hex_digits[(value >> 8)  & 0xF]);
    bios_print_char(hex_digits[(value >> 4)  & 0xF]);
    bios_print_char(hex_digits[(value >> 0)  & 0xF]);
}
static void bios_print_hex16(uint16_t value) {
    bios_print_char(hex_digits[(value >> 12) & 0xF]);
    bios_print_char(hex_digits[(value >> 8)  & 0xF]);
    bios_print_char(hex_digits[(value >> 4)  & 0xF]);
    bios_print_char(hex_digits[(value >> 0)  & 0xF]);
}
static void bios_print_hex8(uint8_t value) {
    bios_print_char(hex_digits[(value >> 4)  & 0xF]);
    bios_print_char(hex_digits[(value >> 0)  & 0xF]);
}
static void bios_hex_dump(void *buffer, int len) {
    for (int i = 0; i < len; i++) {
        bios_print_hex8(*(uint8_t *)buffer++);
        bios_print_char(' ');
    }
}
static void bios_hex16_dump(void *buffer, int len) {
    for (int i = 0; i < len; i++) {
        bios_print_hex16(*(uint16_t *)buffer);
        bios_print_char(' ');
        buffer += 2;
    }
}
static void printf(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(global_buffer, sizeof(global_buffer), fmt, vl);
    va_end(vl);
    for (int i = 0; global_buffer[i] != 0; i++)
        bios_print_char(global_buffer[i]);
}

static inline void panic(char *message) {
    bios_print_str("\r\nPanic: ");
    bios_print_str(message);
    halt();
}


// -------------------------------------------------------

#define SCANCODE_ESCAPE    0x01

static uint16_t bios_ticks_low(void)
{
    // advances ~18.2065 times per second
    uint16_t ticks;
    asm volatile (
        "movb $0x00, %%ah\n"
        "int  $0x1a\n"
        "movw %%dx, %0\n"
        : "=r"(ticks)
        :
        : "ax", "cx", "dx"
    );
    return ticks;
}
static int bios_key_available(void)
{
    unsigned char zf;
    asm volatile (
        "movb $0x01, %%ah\n"
        "int  $0x16\n"
        "setz %0\n"
        : "=r"(zf)
        :
        : "ax"
    );
    return !zf;   /* 1 = key available */
}
static uint16_t bios_read_key(void)
{
    uint16_t key;
    asm volatile (
        "movb $0x00, %%ah\n"
        "int  $0x16\n"
        "movw %%ax, %0\n"
        : "=r"(key)
        :
        : "ax"
    );
    return key;   /* AH=scancode, AL=ASCII */
}
static int get_key_with_timeout(unsigned seconds, uint8_t *scancode, uint8_t *ascii) {
    unsigned deadline = bios_ticks_low() + 18 * seconds;
    unsigned ticks = 0;
    while (seconds == 0 || ticks < deadline) {
        if (bios_key_available()) {
            uint16_t combined = bios_read_key();
            if (scancode != 0) *scancode = combined >> 8;
            if (ascii != 0)    *ascii = combined & 0xFF;
            return 1;
        }
        ticks = bios_ticks_low();
    }
    return 0; // timed out
}

// -------------------------------------------------------

typedef struct vbe_ctrl_info_t vbe_ctrl_info_t;
typedef struct vbe_mode_info_t vbe_mode_info_t;

struct __attribute__((packed)) vbe_ctrl_info_t {
    char     vbe_signature[4];     // 'VBE2', 'VESA' in response
    uint16_t vbe_version;          // BCD, e.g., 0x0200 for VBE 2.0 */
    uint32_t oem_string_ptr;       // physical pointer (seg:off) to OEM string */
    uint8_t  capabilities[4];      // capability bits */
    uint32_t video_mode_ptr;       // pointer to list of supported video modes */
    uint16_t total_memory;         // in 64KB blocks */
    uint8_t  reserved[492];
};

struct __attribute__((packed)) vbe_mode_info_t {
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes;
	uint8_t bpp;			// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;
	uint8_t reserved0;

	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;

	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
};

static uint8_t vbe_set_mode_c(uint16_t mode) {
    uint8_t result;
    asm volatile (
        "movw %[m], %%bx\n"   /* mode -> BX */
        "call vbe_set_mode_real\n"
        "movb %%al, %[r]\n"
        : [r] "=r"(result)
        : [m] "r"(mode)
        : "bx", "ax", "memory"
    );
    return result;
}

// -------------------------------------------------------

static inline void set_pixel(int x, int y, uint32_t color) {
    uint8_t *pix_start = (uint8_t *)(boot_info.fb.fb_addr + y * boot_info.fb.pitch + x * 3);
    pix_start[0] = (color >> 16) & 0xFF;
    pix_start[1] = (color >>  8) & 0xFF;
    pix_start[2] = (color >>  0) & 0xFF;
}
static inline void rect_border(int x1, int y1, int x2, int y2, uint32_t color) {
    for (int x = x1; x <= x2; x++) {
        set_pixel(x, y1, color);
        set_pixel(x, y2, color);
    }
    for (int y = y1; y <= y2; y++) {
        set_pixel(x1, y, color);
        set_pixel(x2, y, color);
    }
}
static inline void rect_filled(int x1, int y1, int x2, int y2, uint32_t color) {
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            set_pixel(x, y, color);
        }
    }
}

static int discover_graphics_modes() {
    vbe_ctrl_info_t vbe_info;
    vbe_mode_info_t mode_info;
    int max_width = 0;
    int max_bpp = 0;
    uint16_t mode1920x1080x32 = 0;
    uint16_t mode1920x1080x24 = 0;
    uint16_t mode1280x720x32  = 0;
    uint16_t mode1280x720x24  = 0;
    uint16_t mode1024x768x32  = 0;
    uint16_t mode1024x768x24  = 0;
    uint16_t mode800x600      = 0;
    uint16_t mode640x480      = 0;

    if (!vbe_get_ctrl_info_real(&vbe_info))
        panic("Could not get VBE controller info");
    
    if (vbe_info.vbe_signature[0] != 'V' || vbe_info.vbe_signature[1] != 'E' || vbe_info.vbe_signature[2] != 'S' || vbe_info.vbe_signature[3] != 'A')
        panic("VBE controller did not return VESA information");
    // bios_hex_dump(&vbe_info, sizeof(vbe_info));

    // this is a pointer to an array of 16 bit words, each with one mode number. The last entry has 0xFFFF for value
    uint16_t *modes_array = (uint16_t *)vbe_info.video_mode_ptr; // in qemu this was 0x000079d4
    for (; *modes_array != 0xFFFF; modes_array++) {
        uint16_t mode = *modes_array;

        if (!vbe_get_mode_info_real(mode, &mode_info))
            panic("Failed getting specific mode info");

        if (mode_info.attributes & 0x80 == 0) { // bit 7 means there's a framebuffer
            // bios_print_str(" (no framebuffer)\r\n");
            continue;
        }
        if (mode_info.memory_model != 6) { 
            // 0: TEXT_MODE: Standard text mode (character cells, not pixel‑addressable).
            // 1: CGA: CGA graphics mode memory layout.
            // 2: HERCULES: Hercules graphics adapter memory layout (monochrome), planar.
            // 3: PLANAR: Planar graphics mode (like VGA planar 4‑plane modes).
            // 4: PACKED: Packed pixel (“chunky”) mode where each pixel’s bits are packed consecutively in memory (e.g., 8‑bpp).
            // 5: NON_CHAIN_4: Non‑chain‑4 mode (a 256‑color mode variant that isn’t VGA “chain 4”).
            // 6: DIRECT_COLOR: Direct color mode (true color/“direct” — separate red/green/blue bitfields).
            // 7: YUV: YUV color mode (representing luminance/chrominance formats).
            // bios_print_str(" (unsupported memory model)\r\n");
            continue;
        }
        if (mode_info.red_mask != 8 || mode_info.green_mask != 8 || mode_info.blue_mask != 8) {
            // bios_print_str(" (unsupported rgb masks)\r\n");
            continue;
        }
        // printf("mode 0x%04x (%4d x %4d, %2d bpp, RGB masks %d/%d/%d)\r\n", mode, mode_info.width, mode_info.height, mode_info.bpp, mode_info.red_mask, mode_info.green_mask, mode_info.blue_mask);

        // save this for possible menu
        if (supported_vbe_modes_count < sizeof(supported_vbe_modes)/sizeof(supported_vbe_modes[0]))
            supported_vbe_modes[supported_vbe_modes_count++] = mode;

        if      (mode_info.width == 1920 && mode_info.height == 1080 && mode_info.bpp == 32) mode1920x1080x32 = mode;
        else if (mode_info.width == 1920 && mode_info.height == 1080 && mode_info.bpp == 24) mode1920x1080x24 = mode;
        else if (mode_info.width == 1280 && mode_info.height ==  720 && mode_info.bpp == 32) mode1280x720x32  = mode;
        else if (mode_info.width == 1280 && mode_info.height ==  720 && mode_info.bpp == 24) mode1280x720x24  = mode;
        else if (mode_info.width == 1024 && mode_info.height ==  768 && mode_info.bpp == 32) mode1024x768x32  = mode;
        else if (mode_info.width == 1024 && mode_info.height ==  768 && mode_info.bpp == 24) mode1024x768x24  = mode;
        else if (mode_info.width ==  800 && mode_info.height ==  600)                        mode800x600      = mode;
        else if (mode_info.width ==  640 && mode_info.height ==  480)                        mode640x480      = mode;
    }
    
    // now, derive one default graphics mode
    if      (mode1920x1080x32) selected_graphics_mode = mode1920x1080x32;
    else if (mode1920x1080x24) selected_graphics_mode = mode1920x1080x24;
    else if (mode1280x720x32)  selected_graphics_mode = mode1280x720x32;
    else if (mode1280x720x24)  selected_graphics_mode = mode1280x720x24;
    else if (mode1024x768x32)  selected_graphics_mode = mode1024x768x32;
    else if (mode1024x768x24)  selected_graphics_mode = mode1024x768x24;
    else if (mode800x600)      selected_graphics_mode = mode800x600;
    else if (mode640x480)      selected_graphics_mode = mode640x480;
    else panic("Could not detect a default graphics mode");

    printf("Default graphics mode %04x\r\n", selected_graphics_mode);
}

static int enable_graphics_mode() {

    vbe_mode_info_t mode_info;
    // Default graphics mode 018c (2560x1600x32)
    // selected_graphics_mode = 0x10F;       //  320 x 200 x 24
    // selected_graphics_mode = 0x112;    //  640 x 480 x 24
    // selected_graphics_mode = 0x115;    //  800 x 600 x 24
    // selected_graphics_mode = 0x118;    // 1024 x 768 x 24
    // selected_graphics_mode = 0x11b;    // 1280 x 1024 x 24
    // selected_graphics_mode = 0x192;    // 32bpp x 1920 x 1080 (mode 0192)
    // selected_graphics_mode = 0x191;    // 24bpp x 1920 x 1080 (mode 0191) (works)
    if (!vbe_get_mode_info_real(selected_graphics_mode, &mode_info))
        panic("Failed getting selected VBE mode info");

    printf("Info from mode %x: %dx%dx%d, pitch=%d\r\n", selected_graphics_mode, mode_info.width, mode_info.height, mode_info.bpp, mode_info.pitch);

    boot_info.fb.fb_addr = mode_info.framebuffer;
    boot_info.fb.width   = mode_info.width;
    boot_info.fb.height  = mode_info.height;
    boot_info.fb.bpp     = mode_info.bpp;
    boot_info.fb.pitch   = mode_info.pitch;

    if (!vbe_set_mode_c(selected_graphics_mode))
        panic("Failed enabling selected VBE mode");

    return 1;
}

static void graphics_demo() {
    // demonstration!
    uint8_t *fb = (uint8_t *)boot_info.fb.fb_addr;
    for (int y = 0; y < 255; y++) {
        for (int x = 0; x < 255; x++) {
            uint8_t *pix_start = fb + y * boot_info.fb.pitch + x * 3;
            pix_start[0] = x & 0xFF; // blue
            pix_start[1] = y & 0xFF; // green
            pix_start[2] = (y+x) & 0xFF; // red
        }
    }

    rect_filled(260, 0, 280, 20, 0x0000cc);
    rect_filled(280, 0, 300, 20, 0x00cc00);
    rect_filled(300, 0, 320, 20, 0xcc0000);

    rect_border(260, 0, 280, 20, 0xffffff);
    rect_border(280, 0, 300, 20, 0xffffff);
    rect_border(300, 0, 320, 20, 0xffffff);

    rect_filled(260, 20, 280, 40, 0x444444);
    rect_filled(280, 20, 300, 40, 0x888888);
    rect_filled(300, 20, 320, 40, 0xcccccc);

    rect_border(260, 20, 280, 40, 0xffffff);
    rect_border(280, 20, 300, 40, 0xffffff);
    rect_border(300, 20, 320, 40, 0xffffff);
}

// -----------------------------------------------------------------

static inline uint16_t get_ax(void) { uint16_t v; asm volatile("mov %%ax,%0":"=r"(v)); return v; }
static inline uint16_t get_bx(void) { uint16_t v; asm volatile("mov %%bx,%0":"=r"(v)); return v; }
static inline uint16_t get_cx(void) { uint16_t v; asm volatile("mov %%cx,%0":"=r"(v)); return v; }
static inline uint16_t get_dx(void) { uint16_t v; asm volatile("mov %%dx,%0":"=r"(v)); return v; }
static inline uint16_t get_si(void) { uint16_t v; asm volatile("mov %%si,%0":"=r"(v)); return v; }
static inline uint16_t get_di(void) { uint16_t v; asm volatile("mov %%di,%0":"=r"(v)); return v; }
static inline uint16_t get_sp(void) { uint16_t v; asm volatile("mov %%sp,%0":"=r"(v)); return v; }
static inline uint16_t get_bp(void) { uint16_t v; asm volatile("mov %%bp,%0":"=r"(v)); return v; }
static inline uint16_t get_cs(void) { uint16_t v; asm volatile("mov %%cs,%0":"=r"(v)); return v; }
static inline uint16_t get_ds(void) { uint16_t v; asm volatile("mov %%ds,%0":"=r"(v)); return v; }
static inline uint16_t get_es(void) { uint16_t v; asm volatile("mov %%es,%0":"=r"(v)); return v; }
static inline uint16_t get_ss(void) { uint16_t v; asm volatile("mov %%ss,%0":"=r"(v)); return v; }
static inline uint32_t get_eip(void) { uint32_t eip; asm volatile ("call 1f\n"  "1: pop %0\n"  : "=r"(eip)); return eip; }

#define print_cpu_status()  \
    bios_print_str("AX:"); asm volatile("mov %%ax,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("BX:"); asm volatile("mov %%bx,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("CX:"); asm volatile("mov %%cx,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("DX:"); asm volatile("mov %%dx,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("\r\n"); \
    \
    bios_print_str("SI:"); asm volatile("mov %%si,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("DI:"); asm volatile("mov %%di,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("BP:"); asm volatile("mov %%bp,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("SP:"); asm volatile("mov %%sp,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("\r\n"); \
    \
    bios_print_str("CS:"); asm volatile("mov %%cs,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("DS:"); asm volatile("mov %%ds,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("ES:"); asm volatile("mov %%es,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("SS:"); asm volatile("mov %%ss,%0":"=r"(_reg16_)); bios_print_hex16(_reg16_); bios_print_char(' ');  \
    bios_print_str("EIP:"); asm volatile ("call 1f\n"  "1: pop %0\n"  : "=r"(_reg32_)); bios_print_hex32(_reg32_);  \
    bios_print_str("\r\n"); \
    \
    asm volatile("mov %%sp,%0":"=r"(_reg16_)); bios_print_str("Words at SP: "); bios_hex16_dump((void *)_reg16_, 16); bios_print_str("\r\n");
    // bios_print_str("Words at BP: "); bios_hex16_dump((void *)(uint32_t)get_bp(), 16); bios_print_str("\r\n");

// -----------------------------------------------------------------

struct disk_address_packet {
    uint8_t  size;      // must be 0x10
    uint8_t  reserved;
    uint16_t count;     // number of sectors to read
    uint16_t offset;    // offset of destination
    uint16_t segment;   // segment of destination
    uint64_t lba;       // starting LBA
} __attribute__((packed));

static struct disk_address_packet disk_address_packet __attribute__((aligned(16)));

int bios_read_sectors(uint32_t lba, uint16_t count, uint32_t dest)
{
    disk_address_packet.size     = 0x10;
    disk_address_packet.reserved = 0;
    disk_address_packet.count    = count;
    disk_address_packet.offset   = dest & 0xF;
    disk_address_packet.segment  = dest >> 4;
    disk_address_packet.lba      = (uint64_t)lba;

    // print_cpu_status();

    asm volatile (
        "movl %[ptr], %%edx\n"
        "callw bios_read_sectors_asm\n"
        "movb %%al, %[ret]\n"
        : [ret] "=m"(_reg8_)
        : [ptr] "r"(&disk_address_packet)
        : "ax", "bx", "cx", "dx", "si", "memory"
    );    

    // print_cpu_status();

    return _reg8_;
}

int load_kernel() {
    // these defined by the build script
    return bios_read_sectors(KERNEL_FIRST_SECTOR_LBA, KERNEL_SECTORS, KERNEL_LOAD_ADDRESS);
}

// -------------------------------------------------------

void run_assembly_interface_tests() {
    /*
        It seems gcc still converts everything to 32bit and pushes args as 4 bytes each, despite explicitly prototyped as uint16_t.
        sizeof(void *) is 4
        when C calls assembly, prototyped as uint16_t, f(0x1234, 0x5678, 0x9abc), in assembly, [BP+ 4] arg is 0x0000
        when C calls assembly, prototyped as uint16_t, f(0x1234, 0x5678, 0x9abc), in assembly, [BP+ 6] arg is 0x1234
        when C calls assembly, prototyped as uint16_t, f(0x1234, 0x5678, 0x9abc), in assembly, [BP+ 8] arg is 0x0000
        when C calls assembly, prototyped as uint16_t, f(0x1234, 0x5678, 0x9abc), in assembly, [BP+10] arg is 0x5678
        similarly,
        when C calls assembly, prototyped as uint32_t, f(0x01234567, 0x89abcdef), in assembly, [BP+ 4] arg is 0x0000
        when C calls assembly, prototyped as uint32_t, f(0x01234567, 0x89abcdef), in assembly, [BP+ 6] arg is 0x4567
        when C calls assembly, prototyped as uint32_t, f(0x01234567, 0x89abcdef), in assembly, [BP+ 8] arg is 0x0123
        when C calls assembly, prototyped as uint32_t, f(0x01234567, 0x89abcdef), in assembly, [BP+10] arg is 0xcdef

        so:
        - BP+6: low word of 1st arg
        - BP+8: hi word of 1st arg
        - BP+10: low word of 2nd arg,
        - BP+12: hi word of 2nd arg
    */

    // bios_print_str("sizeof(void *) is "); bios_print_int(sizeof(void *)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+ 2] arg is 0x"); bios_print_hex16(asm_return_bp2_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+ 4] arg is 0x"); bios_print_hex16(asm_return_bp4_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+ 6] arg is 0x"); bios_print_hex16(asm_return_bp6_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+ 8] arg is 0x"); bios_print_hex16(asm_return_bp8_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+10] arg is 0x"); bios_print_hex16(asm_return_bp10_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // bios_print_str("is assembly, [BP+12] arg is 0x"); bios_print_hex16(asm_return_bp12_arg(0x01234567, 0x89abcdef)); bios_print_str("\r\n");
    // halt();

    if (sizeof(void *) != 4) panic("C assumed to have 32 bits pointers");
    if (asm_return_bp6_arg(0x44332211,  0x88776655) != 0x2211) panic("C assumed to pass 32 bits values, [BP+6] should be low word of 1st argument");
    if (asm_return_bp8_arg(0x44332211,  0x88776655) != 0x4433) panic("C assumed to pass 32 bits values, [BP+8] should be high word of 1st argument");
    if (asm_return_bp10_arg(0x44332211, 0x88776655) != 0x6655) panic("C assumed to pass 32 bits values, [BP+10] should be low word of 2nd argument");
    if (asm_return_bp12_arg(0x44332211, 0x88776655) != 0x8877) panic("C assumed to pass 32 bits values, [BP+12] should be high word of 2nd argument");
}

// -------------------------------------------------------


int choice_of(int num_of_menu_items) {
    uint8_t scancode, ascii;
    while (1) {
        get_key_with_timeout(0, &scancode, &ascii);
        if (scancode == SCANCODE_ESCAPE)
            return -1; // -1 means escape

        // should convert 1-9, a-z into choices
        if (ascii >= '1' && ascii <= '9') {
            int choice = ascii - '1';
            if (choice < num_of_menu_items)
                return choice;
        } else if (ascii >= 'a' && ascii <= 'z') {
            int choice = 10 + (ascii - 'a');
            if (choice < num_of_menu_items)
                return choice;
        }
    }
}

void graphics_mode_menu() {
    int page_no = 0;
    int page_size = 9;
    int pages_count = (supported_vbe_modes_count + (page_size-1)) / page_size;
    vbe_mode_info_t mode_info;
    uint8_t scancode, ascii;

    while (1) {
        printf("Graphics Mode menu, page %d/%d (selected mode 0x%04x)\r\n", page_no + 1, pages_count, selected_graphics_mode);
        for (int i = 0; i < page_size; i++) {
            int index = page_no * page_size + i;
            if (index >= supported_vbe_modes_count) break;
            if (!vbe_get_mode_info_real(supported_vbe_modes[index], &mode_info))
                panic("Could not fetch vbe information");
            printf("  [%c] mode 0x%04x - %4d x %4d x %d bpp\r\n", ('1' + i), supported_vbe_modes[index], mode_info.width, mode_info.height, mode_info.bpp);
        }
        printf("  [p] prev page, [n] next page, [esc] back\r\n");
        get_key_with_timeout(0, &scancode, &ascii);
        if (scancode == SCANCODE_ESCAPE) {
            break;
        } else if (ascii >= '1' && ascii <= '9') {
            int index = page_no * page_size + (ascii - '1');
            if (index < supported_vbe_modes_count) {
                printf("Setting selected mode to 0x%04x\r\n", supported_vbe_modes[index]);
                selected_graphics_mode = supported_vbe_modes[index];
            }
        } else if (ascii == 'n') {
            if (++page_no >= pages_count) page_no = 0;
        } else if (ascii == 'p') {
            if (--page_no < 0) page_no = pages_count - 1;
        }
    }
}

void bios_diagnostics_menu() {
    while (1) {
        printf("Main menu\r\n");
        printf("   1  - Select graphics mode\r\n");
        printf("   2  - BIOS diagnostics menu\r\n");
        printf("  ESC - Continue to boot\r\n");
        int choice = choice_of(2);
        if      (choice <  0) break;
        else if (choice == 0) { graphics_mode_menu(); }
        else if (choice == 1) { bios_diagnostics_menu(); }
    }
}

void possibly_interactive_menu() {
    printf("Press any key to enter interactive mode...");
    uint8_t got_key = get_key_with_timeout(1, 0, 0);
    printf("\r\n");
    if (!got_key) return;

    while (1) {
        printf("Main menu\r\n");
        printf("   1  - Select graphics mode\r\n");
        printf("   2  - BIOS diagnostics menu\r\n");
        printf("  ESC - Continue to boot\r\n");
        int choice = choice_of(2);
        if      (choice <  0) break;
        else if (choice == 0) { graphics_mode_menu(); }
        else if (choice == 1) { bios_diagnostics_menu(); }
    }
}

void stage2_main(void) {

    // we are still running in real mode
    // we can call BIOS interrupts
    // we have some VGA and some VBE routines
    // our job is to:
    // - query, select, and enter graphics mode
    // - load the kernel into specific memory address
    // - enter protected mode and jump to the kernel entry

    initialize_serial_port(); // for debugging in QEMU, run with "-serial stdio"
    run_assembly_interface_tests();

    bios_print_str("Loading kernel...\r\n");
    if (!load_kernel())
        panic("Failed loading kernel");
    printf("Kernel loaded at 0x%x, boot info address 0x%x\r\n", KERNEL_LOAD_ADDRESS, &boot_info);

    // we should already have decided the default graphics mode...
    discover_graphics_modes();
    possibly_interactive_menu();

    printf("Initializing graphics...\r\n");
    if (!enable_graphics_mode())
        panic("Failed initializing graphics");

    // printf("Initializing protected mode and jumping to kernel...\r\n");
    enter_protected_mode();
}
