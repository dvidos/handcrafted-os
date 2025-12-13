#include <stdint.h>
#include "framebuffer.h"



// global buffers for VBE and framebuffer info
framebuffer_info_t fb_info;
uint8_t vbe_info[256];
static const char *hex_digits = "0123456789abcdef";
int serial_port_initialized = 0;
uint8_t _reg8_;
uint16_t _reg16_;
uint32_t _reg32_;
uint32_t kernel_addr_global;

#define KERNEL_FIRST_SECTOR_LBA      17  // 1 for 1st stage, 16 for second, LBA is zero based
#define KERNEL_SIZE_KB               64  // how many kilobytes to load
#define KERNEL_LOAD_ADDRESS      0x8000  // 32KB decimal, this MUST be below 1MB, and match the kernel.ld

extern uint8_t vbe_set_mode_real(void);
extern uint8_t vbe_get_mode_info_real(void);
extern uint8_t bios_read_sectors_asm(uint32_t dap_ptr);
extern void enter_protected_mode(); // make sure arg is uint32_t, a long pointer

// -------------------------------------------------------

static inline uint8_t vbe_set_mode_c(uint16_t mode) {
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
static inline uint8_t vbe_get_mode_info_c(uint16_t mode, void *buffer) {
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
        : "cx", "dx", "ax", "di", "memory"
    );
    return result;
}

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

static void serial_init() {
    outb(0x3F8 + 1, 0x00); // disable interrupts
    outb(0x3F8 + 3, 0x80); // enable DLAB
    outb(0x3F8 + 0, 0x01); // baud divisor low  (115200 / 1 = 115200)
    outb(0x3F8 + 1, 0x00); // baud divisor high
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7); // enable FIFO
    outb(0x3F8 + 4, 0x0B); // IRQs disabled, RTS/DSR set

    serial_port_initialized = 1;
}
static void serial_write_char(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0); // Wait for transmit buffer empty
    outb(0x3F8, c);
}


// -------------------------------------------------------

static void bios_print_char(char c) {
    // bios teletype int 0x10
    asm volatile ("movb $0x0E, %%ah\n" "movb %0, %%al\n"  "int $0x10\n" : : "m"(c) : "ax");

    // for debugging (copy/paste) in QEMU
    if (serial_port_initialized)
        serial_write_char(c);
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

// -------------------------------------------------------

static inline void set_pixel(int x, int y, uint32_t color) {
    uint8_t *pix_start = (uint8_t *)(fb_info.fb_addr + y * fb_info.pitch + x * 3);
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
    uint16_t mode = 0x118; // 1024x768x24
    if (!vbe_get_mode_info_c(mode, vbe_info)) {
        bios_print_str("error getting VBE mode info");
        return 0;
    }

    fb_info.fb_addr = *(uint32_t*)(vbe_info + 0x28);
    fb_info.width   = *(uint16_t*)(vbe_info + 0x12);
    fb_info.height  = *(uint16_t*)(vbe_info + 0x14);
    fb_info.bpp     = *(uint8_t*)(vbe_info + 0x19);
    fb_info.pitch   = *(uint16_t*)(vbe_info + 0x10);

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
    uint8_t *fb = (uint8_t *)fb_info.fb_addr;
    for (int y = 0; y < 255; y++) {
        for (int x = 0; x < 255; x++) {
            uint8_t *pix_start = fb + y * fb_info.pitch + x * 3;
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

struct dap {
    uint8_t  size;      // must be 0x10
    uint8_t  reserved;
    uint16_t count;     // number of sectors to read
    uint16_t offset;    // offset of destination
    uint16_t segment;   // segment of destination
    uint64_t lba;       // starting LBA
} __attribute__((packed));

static struct dap disk_address_packet __attribute__((aligned(16)));

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
        "nop \n"
        "nop \n"
        "nop \n"
        "movl %[ptr], %%edx\n"
        "callw bios_read_sectors_asm\n"
        "movb %%al, %[ret]\n"
        "nop \n"
        "nop \n"
        "nop \n"
        : [ret] "=m"(_reg8_)
        : [ptr] "r"(&disk_address_packet)
        : "ax", "bx", "cx", "dx", "si", "memory"
    );    

    // print_cpu_status();

    return _reg8_;
}

int load_kernel() {
    return bios_read_sectors(
        KERNEL_FIRST_SECTOR_LBA,
        KERNEL_SIZE_KB * 2,      // 0.5kb per sector
        KERNEL_LOAD_ADDRESS
    );
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

    serial_init(); // for debugging in QEMU, run with "-serial stdio"

    bios_print_str("Loading kernel...\r\n");
    if (!load_kernel()) {
        bios_print_str("FAILED");
        halt();
    }
    
    bios_print_str("Initializing graphics...\r\n");
    if (!setup_graphics()) {
        bios_print_str("FAILED");
        halt();
    }
    graphics_demo();


    bios_print_str("Initializing protected mode...\r\n");
    kernel_addr_global = KERNEL_LOAD_ADDRESS;
    enter_protected_mode();
}
