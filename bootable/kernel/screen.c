#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include "string.h"
#include "ports.h"



#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

static int get_cursor_offset() {
    /* Use the VGA ports to get the current cursor position
     * 1. Ask for high byte of the cursor offset (data 14)
     * 2. Ask for low byte (data 15)
     */
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8; /* High byte: << 8 */
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2; /* Position * size of character cell */
}

static void set_cursor_offset(int offset) {
    /* Similar to get_cursor_offset, but instead of reading we write data */
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
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


static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg)
{
	return fg | bg << 4;
}

static inline int get_offset(int col, int row) { return 2 * (row * VGA_WIDTH + col); }
static inline int get_offset_row(int offset) { return offset / (2 * VGA_WIDTH); }
static inline int get_offset_col(int offset) { return (offset - (get_offset_row(offset) * 2 * VGA_WIDTH)) / 2; }


uint8_t screen_row;
uint8_t screen_column;
uint8_t screen_color;
uint8_t* screen_memory;


void screen_init(void)
{
	screen_row = 0;
	screen_column = 0;
	screen_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	screen_memory = (uint8_t*) 0xB8000;
    for (int i = 0; i < (VGA_WIDTH * VGA_HEIGHT) * 2; i += 2) {
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
        screen_memory[i] = screen_memory[i + VGA_WIDTH];
        screen_memory[i + 1] = screen_memory[i + 1 + VGA_WIDTH];
    }
    for (int i = 0; i < VGA_WIDTH; i += 2) {
        screen_memory[(VGA_HEIGHT - 1) + i] = ' ';
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




void _print_aligned(char *buffer, int width, char padder, bool pad_right) {
    width -= strlen(buffer);

    if (!pad_right) {
        while (width-- > 0)
            screen_putchar(padder);
    }

    screen_puts(buffer);

    if (pad_right) {
        while (width-- > 0)
            screen_putchar(padder);
    }
}

// supports:
//   s: string
//   c: char
//   d, i: signed int in decimal
//   u: unsigned int in decimal
//   x: unsigned int in hex
//   o: unsigned int in octal
//   b: unsigned int in binary
//   p: pointer (unsigned int)
//   -: align left / pad right
//   0: pad with zeros, instead of spaces
//   <num>: width of padding
// maybe implemented in the huture:
//   h: short int flag
//   l: long int flag
//   f: float
//   e: scientific
//   .<num>: floating point precision
void printf(const char *format, ...) {

    bool pad_right = false;
    char padder = ' ';
    int width = 0;
    long l;
    unsigned long ul;
    char *ptr;
    char buffer[32];

    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format != '%') {
            screen_putchar(*format);
            format++;
            continue;
        }

        // so definitely a '%'. let's see the next
        format++;
        if (*format == '-') {
            pad_right = true;
            format++;
        }
        if (*format == '0') {
            padder = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        switch (*format) {
            case 'c':
                buffer[0] = (char)va_arg(args, int);
                buffer[1] = '\0';
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 's':
                ptr = va_arg(args, char *);
                _print_aligned(ptr, width, padder, pad_right);
                break;
            case 'd': // signed decimal
            case 'i': // fallthrough
                l = (long)va_arg(args, int);
                ltoa(l, buffer, 10);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 'u': // unsigned decimal
                ul = (unsigned int)va_arg(args, unsigned int);
                ultoa(ul, buffer, 10);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 'x': // hex
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, buffer, 16);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 'o': // octal
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, buffer, 8);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 'b': // binary
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, buffer, 2);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case 'p': // pointer
                ul = (unsigned long)va_arg(args, void *);
                ultoa(ul, buffer, 16);
                _print_aligned(buffer, width, padder, pad_right);
                break;
            case '%':
                screen_putchar(*format);
                break;
            default:
                screen_putchar('%');
                screen_putchar(*format);
                break;
        }

        format++;
    }
    va_end(args);
}
