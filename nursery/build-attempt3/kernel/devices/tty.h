#ifndef _TTY_H
#define _TTY_H

#include "../drivers/kbd_drv.h"

typedef struct tty tty_t;

// for kernel management
void init_tty_manager(int num_of_ttys, int lines_scroll_capacity);
int tty_manager_get_devices_count();
tty_t *tty_manager_get_device(int dev_no);
int tty_get_devno(tty_t *tty);


// called by processes
void tty_read_key(key_event_t *event);
void tty_write(const char *buffer);
int  tty_get_color();
void tty_set_color(int color);
void tty_clear();
void tty_get_cursor(uint8_t *row, uint8_t *col);
void tty_set_cursor(uint8_t row, uint8_t col);
void tty_set_title(const char *title);
void tty_get_dimensions(int *rows, int *cols);
void tty_printf(char *format, ...);



// for processes working on different ttys (not their own process one)
void tty_write_specific_tty(tty_t *tty, const char *buffer, int length);
int  tty_read_specific_tty(tty_t *tty, char *buffer, int length);
void tty_set_title_specific_tty(tty_t *tty, const char *title);


void tty_log_appender(void *context, const char *str);

#endif
