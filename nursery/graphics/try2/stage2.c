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



void stage2_main(void) {
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