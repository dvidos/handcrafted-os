#include "serial.h"
#include "../cpu/ports.h"


static const char *hex_digits = "0123456789abcdef";


void initialize_serial_port() {
    outb(0x3F8 + 1, 0x00); // disable interrupts
    outb(0x3F8 + 3, 0x80); // enable DLAB
    outb(0x3F8 + 0, 0x01); // baud divisor low  (115200 / 1 = 115200)
    outb(0x3F8 + 1, 0x00); // baud divisor high
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7); // enable FIFO
    outb(0x3F8 + 4, 0x0B); // IRQs disabled, RTS/DSR set
}

void serial_print_char(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0); // Wait for transmit buffer empty
    outb(0x3F8, c);
}

void serial_print_str(char *s) {
    while (*s) { serial_print_char(*s); s++; }
}

void serial_print_int(int value) {
    if (value == 0) { serial_print_char('0'); return; }
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
    serial_print_str(buff + idx);
}

void serial_print_hex32(uint32_t value) {
    serial_print_char(hex_digits[(value >> 28) & 0xF]);
    serial_print_char(hex_digits[(value >> 24) & 0xF]);
    serial_print_char(hex_digits[(value >> 20) & 0xF]);
    serial_print_char(hex_digits[(value >> 16) & 0xF]);
    serial_print_char(hex_digits[(value >> 12) & 0xF]);
    serial_print_char(hex_digits[(value >> 8)  & 0xF]);
    serial_print_char(hex_digits[(value >> 4)  & 0xF]);
    serial_print_char(hex_digits[(value >> 0)  & 0xF]);
}

void serial_print_hex16(uint16_t value) {
    serial_print_char(hex_digits[(value >> 12) & 0xF]);
    serial_print_char(hex_digits[(value >> 8)  & 0xF]);
    serial_print_char(hex_digits[(value >> 4)  & 0xF]);
    serial_print_char(hex_digits[(value >> 0)  & 0xF]);
}

void serial_print_hex8(uint8_t value) {
    serial_print_char(hex_digits[(value >> 4)  & 0xF]);
    serial_print_char(hex_digits[(value >> 0)  & 0xF]);
}

void serial_hex_dump(void *buffer, int len) {
    for (int i = 0; i < len; i++) {
        serial_print_hex8(*(uint8_t *)buffer++);
        serial_print_char(' ');
    }
}

void serial_hex16_dump(void *buffer, int len) {
    for (int i = 0; i < len; i++) {
        serial_print_hex16(*(uint16_t *)buffer);
        serial_print_char(' ');
        buffer += 2;
    }
}
