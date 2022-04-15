#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif


void port_byte_out(uint16_t port, uint8_t data) {
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}
uint8_t port_byte_in(uint16_t port) {
    uint8_t result;

    // format: __asm__("assembly" : output operands : input operands)
    __asm__("in %%dx, %%al" : "=a" (result) : "d" (port));

    return result;
}
void port_word_out(uint16_t port, uint16_t data) {
    __asm__("out %%ax, %%dx" : : "a" (data), "d" (port));
}
uint16_t port_word_in(uint16_t port) {
    uint16_t result;

    // format: __asm__("assembly" : output operands : input operands)
    __asm__("in %%dx, %%ax" : "=a" (result) : "d" (port));

    return result;
}


/* Screen i/o ports */
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

int get_cursor_offset() {
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

void set_cursor_offset(int offset) {
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

int strlen(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}


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

void screen_putentryat(char c, uint8_t color, uint8_t col, uint8_t row)
{
    int index = (row * VGA_WIDTH + col) * 2;
	screen_memory[index] = c;
    screen_memory[index + 1] = color;
}

void screen_scroll() {
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

void screen_write(const char* data, int size)
{
	for (int i = 0; i < size; i++)
		screen_putchar(data[i]);
}

void screen_writestring(const char* data)
{
	screen_write(data, strlen(data));
}

void kernel_main(void)
{
	screen_init();

	screen_writestring("Hello, kernel World!\n");
    screen_writestring("This is your programmer calling...\n");
    screen_writestring("Another line!\n");
    screen_writestring("Another line 1!\n");
    screen_writestring("Another line 2!\n");
    // screen_writestring("A");
}


 /**
 * Things to implement from here:
 *
 * also can see chapters 14+ on https://github.com/cfenollosa/os-tutorial
 *
 * - better code structure & make file
 * - better screen handling (Cursor, scroll, etc)
 * - keyboard entry
 * - memory management (heap and -maybe- virtual memory)
 * - disk managmenet of some partition?
 * - multi processing and scheduler (+ timer)
 * - interrupt description table and handlers
 */
