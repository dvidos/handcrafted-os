#include <stdint.h>
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


boot_info_t boot_info;
uint8_t buffer[256];
static const char *hex_digits = "0123456789abcdef";
int serial_port_initialized = 0;
uint8_t _reg8_;
uint16_t _reg16_;
uint32_t _reg32_;


extern uint32_t asm_return_bp2_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp4_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp6_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp8_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp10_arg(uint32_t word1, uint32_t word2);
extern uint32_t asm_return_bp12_arg(uint32_t word1, uint32_t word2);

extern uint8_t vbe_get_ctrl_info_real(void *ptr);
extern uint8_t vbe_set_mode_real(void);
extern uint8_t vbe_get_mode_info_real(void);
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
static inline void panic(char *message) {
    bios_print_str("\r\nPanic: ");
    bios_print_str(message);
    halt();
}


// -------------------------------------------------------

#define SCANCODE_ESCAPE    0x01
#define SCANCODE_NUMROW_1  0x02
#define SCANCODE_NUMROW_2  0x03
#define SCANCODE_NUMROW_3  0x04
#define SCANCODE_NUMROW_4  0x05
#define SCANCODE_NUMROW_5  0x06
#define SCANCODE_NUMROW_6  0x07
#define SCANCODE_NUMROW_7  0x08
#define SCANCODE_NUMROW_8  0x09
#define SCANCODE_NUMROW_9  0x0a
#define SCANCODE_NUMROW_0  0x0b
#define SCANCODE_KEYPAD_1  0x4f
#define SCANCODE_KEYPAD_2  0x50
#define SCANCODE_KEYPAD_3  0x51
#define SCANCODE_KEYPAD_4  0x4b
#define SCANCODE_KEYPAD_5  0x4c
#define SCANCODE_KEYPAD_6  0x4d
#define SCANCODE_KEYPAD_7  0x47
#define SCANCODE_KEYPAD_8  0x48
#define SCANCODE_KEYPAD_9  0x49
#define SCANCODE_KEYPAD_0  0x52

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
static uint16_t get_key_with_timeout(unsigned seconds) {
    unsigned start = bios_ticks_low();
    while (1) {
        if (bios_key_available()) {
            return bios_read_key();
        }

        unsigned ticks = bios_ticks_low();
        if (seconds != 0 && ticks > start + 18 * seconds) {
            return 0; //timed out
        }
    }
}

static void discover_keys() {

    uint32_t key = get_key_with_timeout(1);
    if (key == 0)
        return;
    while (1) {
        bios_print_str("Press any key, esc to exit");
        key = get_key_with_timeout(0);
        uint8_t scancode = key >> 8;
        if (scancode == SCANCODE_ESCAPE)
            break;

        bios_print_str(": scan code 0x"); bios_print_hex8((key >> 8) & 0xFF);
        bios_print_str(", ascii 0x"); bios_print_hex8(key & 0xFF);
        if ((key & 0xFF) >= 32 && (key & 0xFF) < 128) {
            bios_print_str(" ('"); bios_print_char(key & 0xFF); bios_print_str("')"); 
        }
        bios_print_str("\r\n");
    }
}

// -------------------------------------------------------

typedef struct __attribute__((packed)) {
    char     vbe_signature[4];     /* 'VBE2' */
    uint16_t vbe_version;          /* BCD, e.g., 0x0200 for VBE 2.0 */
    uint32_t oem_string_ptr;       /* physical pointer (seg:off) to OEM string */
    uint8_t  capabilities[4];      /* capability bits */
    uint32_t video_mode_ptr;       /* pointer to list of supported video modes */
    uint16_t total_memory;         /* in 64KB blocks */
    uint16_t oem_software_rev;
    uint32_t oem_vendor_name_ptr;
    uint32_t oem_product_name_ptr;
    uint32_t oem_product_rev_ptr;
    uint8_t  reserved[222];        /* reserved / padding */
    uint8_t  oem_data[256];        /* optional OEM scratch area */
} vbe_ctrl_info_t;

static uint8_t vbe_get_mode_info_c(uint16_t mode, void *buffer) {
    uint8_t result;
    uint32_t addr = (uint32_t)buffer;

    /* pass linear address in DX to NASM, NASM splits to ES:DI */
    asm volatile (
        "movl %[addr], %%edx\n"
        "movw %[m], %%cx\n"
        "call vbe_get_mode_info_real\n"
        "movb %%al, %[r]\n"
        : [r] "=r"(result)
        : [addr] "r"(addr), [m] "r"(mode)
        : "cx", "dx", "ax", "bx", "di", "memory"
    );
    return result;
}
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

static int setup_graphics() {

    vbe_ctrl_info_t vbe_info;
    bios_print_str("getting vbe ctrl info ");
    // ((char *)&vbe_ctrl_info)[0] = 'V';
    // ((char *)&vbe_ctrl_info)[1] = 'B';
    // ((char *)&vbe_ctrl_info)[2] = 'E';
    // ((char *)&vbe_ctrl_info)[3] = '2';
    if (vbe_get_ctrl_info_real(&vbe_info)) {
        bios_print_str("success\r\n");
        bios_print_char(vbe_info.vbe_signature[0]);
        bios_print_char(vbe_info.vbe_signature[1]);
        bios_print_char(vbe_info.vbe_signature[2]);
        bios_print_char(vbe_info.vbe_signature[3]);
        bios_print_char(' ');
        bios_print_str("addr:"); bios_print_hex32(vbe_info.video_mode_ptr);
        // bios_hex_dump(&vbe_ctrl_info, sizeof(vbe_ctrl_info_t));
    } else {
        bios_print_str("failure\r\n");
    }
    halt();




    // uint16_t mode = 0x10F;       //  320 x 200 x 24
    uint16_t mode = 0x112;    //  640 x 480 x 24
    // uint16_t mode = 0x115;    //  800 x 600 x 24
    // uint16_t mode = 0x118;    // 1024 x 768 x 24
    // uint16_t mode = 0x11b;    // 1280 x 1024 x 24
    if (!vbe_get_mode_info_c(mode, buffer)) {
        bios_print_str("error getting VBE mode info");
        return 0;
    }

    boot_info.fb.fb_addr = *(uint32_t*)(buffer + 0x28);
    boot_info.fb.width   = *(uint16_t*)(buffer + 0x12);
    boot_info.fb.height  = *(uint16_t*)(buffer + 0x14);
    boot_info.fb.bpp     = *(uint8_t*)(buffer + 0x19);
    boot_info.fb.pitch   = *(uint16_t*)(buffer + 0x10);

    // bios_print_str("VBE mode info\r\n");
    // bios_print_str("bytes:   "); bios_hex_dump(vbe_info, 256); bios_print_str("\r\n");
    // bios_print_str("fb_addr: "); bios_print_hex32(fb_info.fb_addr); bios_print_str("\r\n");
    // bios_print_str("width:   "); bios_print_int(fb_info.width); bios_print_str("\r\n");
    // bios_print_str("height:  "); bios_print_int(fb_info.height); bios_print_str("\r\n");
    // bios_print_str("bpp:     "); bios_print_int(fb_info.bpp);   bios_print_str("\r\n");
    // bios_print_str("pitch:   "); bios_print_int(fb_info.pitch); bios_print_str("\r\n");
    // halt();

    if (!vbe_set_mode_c(mode)) {
        bios_print_str("error setting VBE mode");
        return 0;
    }

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

void assembly_interface_tests() {
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

void stage2_main(void) {

    // we are still running in real mode
    // we can call BIOS interrupts
    // we have some VGA and some VBE routines
    // our job is to:
    // - query, select, and enter graphics mode
    // - load the kernel into specific memory address
    // - enter protected mode and jump to the kernel entry

    initialize_serial_port(); // for debugging in QEMU, run with "-serial stdio"

    assembly_interface_tests();


    bios_print_str("Loading kernel...\r\n");
    if (!load_kernel()) {
        bios_print_str("FAILED");
        halt();
    }
    
    bios_print_str("Kernel at 0x"); bios_print_hex32(KERNEL_LOAD_ADDRESS); 
    bios_print_str(", boot info at 0x"); bios_print_hex32((uint32_t)&boot_info); 
    bios_print_str("\r\n");

    bios_print_str("Press a key to discover keys...");
    discover_keys();
    bios_print_str("\r\n");

    bios_print_str("Initializing graphics...\r\n");
    if (!setup_graphics()) {
        bios_print_str("FAILED");
        halt();
    }

    // bios_print_str("Initializing protected mode and jumping to kernel...\r\n");
    enter_protected_mode();
}
