#include <stdint.h>
#include "framebuffer.h"


// global buffers for VBE and framebuffer info
framebuffer_info_t fb_info;
uint8_t vbe_info[256];
static const char *hex_digits = "0123456789abcdef";
int echo_in_serial_port = 0;

#define KERNEL_LOAD_ADDR 0x8000  /* 32KB */
extern void pm_entry(uint32_t kernel_addr);

// -------------------------------------------------------

extern uint8_t vbe_set_mode_real(void);
extern uint8_t vbe_get_mode_info_real(void);

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
    if (echo_in_serial_port)
        serial_write_char(c);
}
static void bios_print_str(char *s) {
    while (*s) bios_print_char(*s++);
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
static void bios_hex_dump(unsigned char *buffer, int len) {
    for (int i = 0; i < len; i++) {
        bios_print_hex8(*buffer++);
        bios_print_char(' ');
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

    return 1;
}

// -----------------------------------------------------------------

void stage2_main(void) {

    serial_init(); // for debugging in QEMU, run with "-serial stdio"
    echo_in_serial_port = 1;

    bios_print_str("\r\nStage 2 running... ");
    if (!setup_graphics())
        halt();


    halt();

    // we should load the kernel as well, shouldn't we????
    // and we should pass in framebuffer & boot information, right?
    // we should push the fb_info address onto the stack though...
    // load 128 sectors, i.e. 64KB into address 0x8000 or 32KB.

    // now enter Protected Mode and jump directly to the kernel
    // passing in any information we have now
    pm_entry(KERNEL_LOAD_ADDR);
}
