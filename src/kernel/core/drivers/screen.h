#ifndef _SCREEN_H
#define _SCREEN_H

#include <stdint.h>
#include <stdbool.h>


void screen_init(void);
void screen_setcolor(uint8_t color);
uint8_t screen_getcolor();
void screen_clear();
void screen_writen(const char* data, int size);
void screen_write(const char* data);
void screen_putchar(const char c);

void printf(char *format, ...);
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
