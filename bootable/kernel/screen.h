#ifndef _SCREEN_H
#define _SCREEN_H

void screen_init(void);
void screen_setcolor(uint8_t color);
void screen_clear();
void screen_writen(const char* data, int size);
void screen_write(const char* data);

#endif 

