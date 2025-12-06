#pragma once
#include <stdint.h>

void vga_char(char c);
void vga_str(const char* str);
void vga_int(int value);
void vga_hex8(uint8_t val);
void vga_hex32(uint32_t val);
