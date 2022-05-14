#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include "string.h"
#include "ports.h"
#include "cpu.h"



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

#define VGA_COLOR_BLACK           0
#define VGA_COLOR_BLUE            1
#define VGA_COLOR_GREEN           2
#define VGA_COLOR_CYAN            3
#define VGA_COLOR_RED             4
#define VGA_COLOR_MAGENTA         5
#define VGA_COLOR_BROWN           6
#define VGA_COLOR_LIGHT_GREY      7
#define VGA_COLOR_DARK_GREY       8
#define VGA_COLOR_LIGHT_BLUE      9
#define VGA_COLOR_LIGHT_GREEN    10
#define VGA_COLOR_LIGHT_CYAN     11
#define VGA_COLOR_LIGHT_RED      12
#define VGA_COLOR_LIGHT_MAGENTA  13
#define VGA_COLOR_LIGHT_BROWN    14
#define VGA_COLOR_WHITE          15

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


void screen_init(void)
{
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

void screen_setcolor(uint8_t color)
{
	screen_color = color;
}

static void screen_putentryat(char c, uint8_t color, uint8_t col, uint8_t row)
{
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

static void screen_putchar(char c)
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

void screen_puts(const char* data)
{
	screen_writen(data, strlen(data));
}

void panic(char *message) {
    cli();
    screen_puts("\nKernel panic: ");
    screen_puts(message);
    for (;;)
        __asm__ volatile("hlt");    
}

void printf(const char *format, ...) {
    char buffer[128];

    va_list args;
    va_start(args, format);
    vsprintfn(buffer, sizeof(buffer), format, args);
    va_end(args);

    screen_puts(buffer);
}

static inline char dot(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}


void memdump(void *address, int bytes, bool decreasing) {
    unsigned char *ptr = (unsigned char *)address;
    int dir = decreasing ? -1 : 1;
    while (bytes > 0) {
        // using xxd's format, seems nice
        printf("%08p: %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x  %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
            ptr,
            ptr[0], ptr[1 * dir], ptr[2 * dir], ptr[3 * dir], 
            ptr[4 * dir], ptr[5 * dir], ptr[6 * dir], ptr[7 * dir],
            ptr[8 * dir], ptr[9 * dir], ptr[10 * dir], ptr[11 * dir], 
            ptr[12 * dir], ptr[13 * dir], ptr[14 * dir], ptr[15 * dir],
            dot(ptr[0]), dot(ptr[1 * dir]), dot(ptr[2 * dir]), dot(ptr[3 * dir]),
            dot(ptr[4 * dir]), dot(ptr[5 * dir]), dot(ptr[6 * dir]), dot(ptr[7 * dir]),
            dot(ptr[8 * dir]), dot(ptr[9 * dir]), dot(ptr[10 * dir]), dot(ptr[11 * dir]),
            dot(ptr[12 * dir]), dot(ptr[13 * dir]), dot(ptr[14 * dir]), dot(ptr[15 * dir])
        );
        ptr += 16 * dir;
        bytes -= 16;
    }
}