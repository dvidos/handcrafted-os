#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



void outb(uint16_t port, uint8_t data) {
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}

uint8_t inb(uint16_t port) {
    uint8_t result;

    // format: __asm__("assembly" : output operands : input operands)
    __asm__("in %%dx, %%al" : "=a" (result) : "d" (port));

    return result;
}

void outw(uint16_t port, uint16_t data) {
    __asm__("out %%ax, %%dx" : : "a" (data), "d" (port));
}

uint16_t inw(uint16_t port) {
    uint16_t result;

    // format: __asm__("assembly" : output operands : input operands)
    __asm__("in %%dx, %%ax" : "=a" (result) : "d" (port));

    return result;
}


