#ifndef _STDIO_H
#define _STDIO_H

// key codes and the key_event_t type
#include <keyboard.h>


#define EOF (-1)

// prints to current tty
int printf(const char* restrict format, ...);

// prints a message on the tty. no new line is added
int puts(char *message);

// prints one char on the tty
int putchar(int c);

// clears the screen of the tty
int clear_screen();

// returns cursor information
int where_xy(int *x, int *y);

// positions cursor
int goto_xy(int x, int y);

// returns screen dimensions
int screen_dimensions(int *cols, int *rows);

// returns current screen color
int get_screen_color();

// sets screen color (bg = high nibble)
int set_screen_color(int color);


#define COLOR_BLACK           0
#define COLOR_BLUE            1
#define COLOR_GREEN           2
#define COLOR_CYAN            3
#define COLOR_RED             4
#define COLOR_MAGENTA         5
#define COLOR_BROWN           6
#define COLOR_LIGHT_GREY      7
#define COLOR_DARK_GREY       8
#define COLOR_LIGHT_BLUE      9
#define COLOR_LIGHT_GREEN    10
#define COLOR_LIGHT_CYAN     11
#define COLOR_LIGHT_RED      12
#define COLOR_LIGHT_MAGENTA  13
#define COLOR_LIGHT_BROWN    14
#define COLOR_WHITE          15


void getkey(key_event_t *event);


#endif
