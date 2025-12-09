#include <stdint.h>
#include "framebuffer.h"


/* global buffer for framebuffer info */
framebuffer_info_t fb_info;

/* global buffer for VBE mode info */
uint8_t vbe_info[256];

#define KERNEL_LOAD_ADDR 0x00100000  /* 1 MB */
extern void pm_entry(uint32_t kernel_addr);

// -------------------------------------------------------

/* external NASM routines */
extern uint8_t vbe_set_mode_real(void);
extern uint8_t vbe_get_mode_info_real(void);

/* set VBE mode */
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

void inline bios_print_char(char c) {
    asm volatile (
        "movb $0x0E, %%ah\n"   // BIOS teletype function
        "movb %0, %%al\n"      // character to print
        "int $0x10\n"
        :
        : "r"(c)
        : "ax"
    );
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

// -------------------------------------------------------

void stage2_main(void) {
    bios_print_str("\r\nStage 2 running...");

    for(;;) asm("hlt");


    uint16_t mode = 0x118;
    if (!vbe_get_mode_info_c(mode, vbe_info))
        for(;;) asm("hlt");

    fb_info.fb_addr = *(uint32_t*)(vbe_info + 0x0C);
    fb_info.width   = *(uint16_t*)(vbe_info + 0x12);
    fb_info.height  = *(uint16_t*)(vbe_info + 0x14);
    fb_info.bpp     = *(uint8_t*)(vbe_info + 0x19);
    fb_info.pitch   = *(uint16_t*)(vbe_info + 0x10);

    if (!vbe_set_mode_c(mode))
        for(;;) asm("hlt");

    pm_entry(KERNEL_LOAD_ADDR);
}