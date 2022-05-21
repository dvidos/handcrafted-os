#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include "../string.h"
#include "../cpu.h"
#include "../cpu.h"
#include "screen.h"


// lots of useful information here:
// https://web.stanford.edu/class/cs140/projects/pintos/specs/freevga/home.htm


struct vga_char {
    uint8_t blink: 1;
    uint8_t bgcolor: 3;
    uint8_t fg_color: 4;
    uint8_t character: 8;
} __attribute__((packed));

// hopefully we can support the following:
// - functions that represent VGA functionality (e.g. fast scrolling)
// - maybe load greek fonts some day (utf-8 support)
// i don't care too much about changing the color palette, fonts, glyphs, num of lines etc
//
// at a higher level, we'd like to be able to maintain various buffers, for various ttys
// and scroll horizontally between them (the way macos is doing)
// also, possibly be able to scroll back a number of lines
// but that all is the realm of a tty device (do we want this?)


#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

static int get_cursor_offset() {
    /* Use the VGA ports to get the current cursor position
     * 1. Ask for high byte of the cursor offset (data 14)
     * 2. Ask for low byte (data 15)
     */
    outb(REG_SCREEN_CTRL, 14);
    int offset = inb(REG_SCREEN_DATA) << 8; /* High byte: << 8 */
    outb(REG_SCREEN_CTRL, 15);
    offset += inb(REG_SCREEN_DATA);
    return offset * 2; /* Position * size of character cell */
}

static void set_cursor_offset(int offset) {
    /* Similar to get_cursor_offset, but instead of reading we write data */
    offset /= 2;
    outb(REG_SCREEN_CTRL, 14);
    outb(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    outb(REG_SCREEN_CTRL, 15);
    outb(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

#define VGA_WIDTH  80
#define VGA_HEIGHT 25


uint8_t screen_row;
uint8_t screen_column;
uint8_t screen_color;
uint8_t *screen_memory;

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }

static inline void put_char(int col, int row, uint8_t chr) { screen_memory[(row * VGA_WIDTH + col) * 2] = chr; }
static inline uint8_t get_char(int col, int row) { return screen_memory[(row * VGA_WIDTH + col) * 2]; }
static inline void put_color(int col, int row, uint8_t color) { screen_memory[(row * VGA_WIDTH + col) * 2 + 1] = color; }
static inline uint8_t get_color(int col, int row) { return screen_memory[(row * VGA_WIDTH + col) * 2 + 1]; }

static inline int get_offset(int col, int row) { return 2 * (row * VGA_WIDTH + col); }
static inline void get_row_col(int *col, int *row, int offset) { *col = offset % (VGA_WIDTH*2); *row = offset / (VGA_WIDTH*2); }


void screen_init(void) {
	screen_row = 0;
	screen_column = 0;
	screen_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	screen_memory = (uint8_t *)0xB8000;
    for (int i = 0; i < (VGA_WIDTH * VGA_HEIGHT) * 2; i += 2) {
        #pragma GCC diagnostic ignored "-Wstringop-overflow"
        screen_memory[i] = ' ';
        screen_memory[i + 1] = screen_color;
    }
    set_cursor_offset(0);
}

uint8_t screen_getcolor() {
	return screen_color;
}

void screen_setcolor(uint8_t color) {
	screen_color = color;
}

static void screen_putentryat(char c, uint8_t color, uint8_t col, uint8_t row) {
    int index = (row * VGA_WIDTH + col) * 2;
	screen_memory[index] = c;
    screen_memory[index + 1] = color;
}

static void screen_scroll() {
    for (int i = 0; i < (VGA_WIDTH * (VGA_HEIGHT - 1)) * 2; i += 2) {
        screen_memory[i] = screen_memory[i + (VGA_WIDTH * 2)];
        screen_memory[i + 1] = screen_memory[i + 1 + (VGA_WIDTH * 2)];
    }
    for (int i = 0; i < VGA_WIDTH * 2; i += 2) {
        screen_memory[((VGA_HEIGHT - 1) * VGA_WIDTH) * 2 + i] = ' ';
    }
}

void screen_clear() {
    for (int i = 0; i < (VGA_WIDTH * VGA_HEIGHT) * 2; i += 2) {
        screen_memory[i] = ' ';
        screen_memory[i + 1] = screen_color;
    }
    screen_row = 0;
    screen_column = 0;
    set_cursor_offset(0);
}

void screen_putchar(char c)
{
    if (c == '\n') {
    	if (++screen_row == VGA_HEIGHT) {
            screen_scroll();
            screen_row--;
        }
        screen_column = 0;
    } else if (c == '\r') {
        screen_column = 0;
    } else if (c == '\b') {
        if (screen_column > 0)
            screen_column--;
    } else if (c == '\t') {
        screen_column = ((screen_column / 8) + 1) * 8;
    } else {
        screen_putentryat(c, screen_color, screen_column, screen_row);
    	if (++screen_column == VGA_WIDTH) {
    		screen_column = 0;
    		if (++screen_row == VGA_HEIGHT) {
                screen_scroll();
                screen_row--;
            }
    	}
    }

    set_cursor_offset((screen_row * VGA_WIDTH + screen_column) * 2);
}

void screen_writen(const char* data, int size)
{
	for (int i = 0; i < size; i++)
		screen_putchar(data[i]);
}

void screen_write(const char* data)
{
	screen_writen(data, strlen(data));
}

void panic(char *message) {
    cli();
    screen_write("\nKernel panic: ");
    screen_write(message);
    for (;;)
        __asm__ volatile("hlt");    
}

void printf(char *format, ...) {
    char buffer[128];

    va_list args;
    va_start(args, format);
    vsprintfn(buffer, sizeof(buffer), format, args);
    va_end(args);

    screen_write(buffer);
}

static inline char dot(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}
