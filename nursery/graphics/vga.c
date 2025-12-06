#include "vga.h"
#include "serial.h"


#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xB8000

static uint16_t* vga_buffer = (uint16_t*) VGA_ADDRESS;
static uint8_t vga_row = 0;
static uint8_t vga_col = 0;
static uint8_t vga_color = 0x0F; // White on black
static const char *hex = "0123456789ABCDEF";



void vga_char(char c) {
    // echo to serial, to allow for copy/pasting.
    char buff[2];
    buff[0] = c;
    buff[1] = 0;
    serial_write(buff);


    if (c == '\n') {
        vga_col = 0;
        if (++vga_row >= VGA_HEIGHT) vga_row = 0;
        return;
    }
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = ((uint16_t)vga_color << 8) | c;
    if (++vga_col >= VGA_WIDTH) {
        vga_col = 0;
        if (++vga_row >= VGA_HEIGHT) vga_row = 0;
    }
}
void vga_str(const char* str) {
    while (*str) vga_char(*str++);
}

void vga_int(int value) {
    char buffer[12]; // enough for -2^31 and null
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        vga_char('0');
        return;
    }

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    // Convert to string in reverse
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    if (is_negative)
        buffer[i++] = '-';
    for (int j = i - 1; j >= 0; j--)
        vga_char(buffer[j]);
}

void vga_hex8(uint8_t val) {
    const char *hex = "0123456789ABCDEF";
    vga_char(hex[(val >> 4) & 0xF]);
    vga_char(hex[val & 0xF]);
}


void vga_hex32(uint32_t val) {
    vga_char(hex[(val >> 28) & 0xF]);
    vga_char(hex[(val >> 24) & 0xF]);
    vga_char(hex[(val >> 20) & 0xF]);
    vga_char(hex[(val >> 16) & 0xF]);
    vga_char(hex[(val >> 12) & 0xF]);
    vga_char(hex[(val >> 8) & 0xF]);
    vga_char(hex[(val >> 4) & 0xF]);
    vga_char(hex[val & 0xF]);
}
