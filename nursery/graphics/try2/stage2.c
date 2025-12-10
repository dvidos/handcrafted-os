#include <stdint.h>
#include "framebuffer.h"


/* global buffer for framebuffer info */
framebuffer_info_t fb_info;

/* global buffer for VBE mode info */
uint8_t vbe_info[256];

#define KERNEL_LOAD_ADDR 0x00100000  /* 1 MB */
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

/* get VBE mode info */
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

void serial_init() {
    outb(0x3F8 + 1, 0x00); // disable interrupts
    outb(0x3F8 + 3, 0x80); // enable DLAB
    outb(0x3F8 + 0, 0x01); // baud divisor low  (115200 / 1 = 115200)
    outb(0x3F8 + 1, 0x00); // baud divisor high
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7); // enable FIFO
    outb(0x3F8 + 4, 0x0B); // IRQs disabled, RTS/DSR set
}
void serial_write_char(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0); // Wait for transmit buffer empty
    outb(0x3F8, c);
}

// -------------------------------------------------------

void bios_print_char(char c) {
    // bios teletype int 0x10
    asm volatile ("movb $0x0E, %%ah\n" "movb %0, %%al\n"  "int $0x10\n" : : "m"(c) : "ax");

    // for debugging (copy/paste) in QEMU
    serial_write_char(c);
}
void bios_print_str(char *s) {
    while (*s) bios_print_char(*s++);
}
void bios_print_int(int value) {
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
static const char *hex = "0123456789abcdef";
void bios_print_hex32(uint32_t value) {
    bios_print_char(hex[(value >> 28) & 0xF]);
    bios_print_char(hex[(value >> 24) & 0xF]);
    bios_print_char(hex[(value >> 20) & 0xF]);
    bios_print_char(hex[(value >> 16) & 0xF]);
    bios_print_char(hex[(value >> 12) & 0xF]);
    bios_print_char(hex[(value >> 8)  & 0xF]);
    bios_print_char(hex[(value >> 4)  & 0xF]);
    bios_print_char(hex[(value >> 0)  & 0xF]);
}
void bios_print_hex16(uint16_t value) {
    bios_print_char(hex[(value >> 12) & 0xF]);
    bios_print_char(hex[(value >> 8)  & 0xF]);
    bios_print_char(hex[(value >> 4)  & 0xF]);
    bios_print_char(hex[(value >> 0)  & 0xF]);
}
void bios_print_hex8(uint8_t value) {
    bios_print_char(hex[(value >> 4)  & 0xF]);
    bios_print_char(hex[(value >> 0)  & 0xF]);
}
void bios_hex_dump(unsigned char *buffer, int len) {
    for (int i = 0; i < len; i++) {
        bios_print_hex8(*buffer++);
        bios_print_char(' ');
    }
}

// -------------------------------------------------------

int setup_graphics() {
    uint16_t mode = 0x118; // 1024x768x24
    if (!vbe_get_mode_info_c(mode, vbe_info)) {
        bios_print_str("error getting VBE mode info");
        return 0;
    }

    uint32_t fb_addr = *(uint32_t*)(vbe_info + 0x28);
    uint16_t width   = *(uint16_t*)(vbe_info + 0x12);
    uint16_t height  = *(uint16_t*)(vbe_info + 0x14);
    uint8_t  bpp     = *(uint8_t*)(vbe_info + 0x19);
    uint16_t pitch   = *(uint16_t*)(vbe_info + 0x10);

    // bios_print_str("VBE mode info\r\n");
    // bios_print_str("bytes:   "); bios_hex_dump(vbe_info, 256); bios_print_str("\r\n");
    // bios_print_str("fb_addr: "); bios_print_hex32(fb_addr); bios_print_str("\r\n");
    // bios_print_str("width:   "); bios_print_int(width); bios_print_str("\r\n");
    // bios_print_str("height:  "); bios_print_int(height); bios_print_str("\r\n");
    // bios_print_str("bpp:     "); bios_print_int(bpp);   bios_print_str("\r\n");
    // bios_print_str("pitch:   "); bios_print_int(pitch); bios_print_str("\r\n");

    // fb_info.fb_addr = *(uint32_t*)(vbe_info + 0x0C);
    // fb_info.width   = *(uint16_t*)(vbe_info + 0x12);
    // fb_info.height  = *(uint16_t*)(vbe_info + 0x14);
    // fb_info.bpp     = *(uint8_t*)(vbe_info + 0x19);
    // fb_info.pitch   = *(uint16_t*)(vbe_info + 0x10);

    if (!vbe_set_mode_c(mode)) {
        bios_print_str("error setting VBE mode");
        return 0;
    }

    // demonstration!
    uint8_t *fb = (uint8_t *)fb_addr;
    for (int y = 0; y < 255; y++) {
        for (int x = 0; x < 255; x++) {
            uint8_t *pix_start = fb + y * pitch + x * 3;
            pix_start[0] = x & 0xFF; // blue
            pix_start[1] = y & 0xFF; // green
            pix_start[2] = (y+x) & 0xFF; // red
        }
    }

    return 1;
}

void stage2_main(void) {

    serial_init(); // for debugging in QEMU

    bios_print_str("\r\nStage 2 running... ");
    if (!setup_graphics()) halt();

    halt();

    // we should load the kernel as well, shouldn't we????
    // and we should pass in framebuffer & boot information, right?
    pm_entry(KERNEL_LOAD_ADDR);
}
