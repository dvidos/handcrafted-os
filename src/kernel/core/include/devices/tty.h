#ifndef _TTY_H
#define _TTY_H

#include <drivers/keyboard.h>

typedef struct tty tty_t;

// for kernel management
void init_tty_manager(int num_of_ttys, int lines_scroll_capacity);
tty_t *tty_manager_get_device(int dev_no);




// called by processes
void tty_read_key(key_event_t *event);
void tty_write(char *buffer);
int tty_get_color();
void tty_set_color(int color);
void tty_clear();
void tty_get_cursor(uint8_t *row, uint8_t *col);
void tty_set_cursor(uint8_t row, uint8_t col);
void tty_set_title(char *title);
void tty_get_dimensions(int *rows, int *cols);

// print to current tty
void printf(char *format, ...);

// for processes working on different ttys (not their own process one)
void tty_write_specific_tty(tty_t *tty, char *buffer);
void tty_set_title_specific_tty(tty_t *tty, char *title);

#endif
