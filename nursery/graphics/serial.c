#include <stdint.h>

#define COM1 0x3F8


// Minimal I/O port access
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void serial_write_char(char c) {
    // wait for transmit holding register empty
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}

void serial_write(const char* str) {
    for (const char* p = str; *p; p++)
        serial_write_char(*p);
}

