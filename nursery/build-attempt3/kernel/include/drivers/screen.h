#ifndef _SCREEN_H
#define _SCREEN_H

#include <ctypes.h>


void screen_init(void);

int screen_rows();
int screen_cols();

void screen_clear();
void screen_write(char* data);

uint8_t screen_get_color();
void screen_set_color(uint8_t color);

void screen_get_cursor(uint8_t *row, uint8_t *col);
void screen_set_cursor(uint8_t row, uint8_t col);

// for ttys to manipulate screen
void screen_draw_char_at(char c, uint8_t color, uint8_t col, uint8_t row);
void screen_draw_str_at(char *str, uint8_t color, uint8_t col, uint8_t row);
void screen_draw_full_row(char c, uint8_t color, uint8_t row);
void screen_copy_buffer_to_screen(char *buffer, int length, uint8_t col, uint8_t row);

// print to screen
void printk(char *format, ...); 
void panic(char *message);


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



#endif
