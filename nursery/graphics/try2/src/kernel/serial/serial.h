#pragma once
#include <stdint.h>


void serial_init();
void serial_print_char(char c);
void serial_print_str(char *s);
void serial_print_int(int value);
void serial_print_hex32(uint32_t value);
void serial_print_hex16(uint16_t value);
void serial_print_hex8(uint8_t value);
void serial_hex_dump(void *buffer, int len);
void serial_hex16_dump(void *buffer, int len);

