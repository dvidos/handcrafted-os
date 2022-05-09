#ifndef _SCREEN_H
#define _SCREEN_H

#include <stdint.h>
#include <stdbool.h>


void screen_init(void);
void screen_setcolor(uint8_t color);
void screen_clear();
void screen_writen(const char* data, int size);
void screen_write(const char* data);
void screen_puts(const char* data);
void screen_putchar(const char c);

void printf(char *format, ...);
void panic(char *message);
void dumpmem(void *address, int bytes, bool decreasing);


#endif
