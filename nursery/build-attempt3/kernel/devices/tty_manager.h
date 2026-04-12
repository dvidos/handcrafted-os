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
int  tty_get_color(tty_t *tty);
void tty_set_color(tty_t *tty, int color);
void tty_clear_screen(tty_t *tty);
void tty_get_cursor(tty_t *tty, uint8_t *row, uint8_t *col);
void tty_set_cursor(tty_t *tty, uint8_t row, uint8_t col);
void tty_set_title(tty_t *tty, const char *title);
void tty_get_dimensions(tty_t *tty, int *rows, int *cols);
void tty_printf(tty_t *tty, char *format, ...);



// for processes working on different ttys (not their own process one)
void tty_set_title(tty_t *tty, const char *title);
void tty_write(tty_t *tty, const char *buffer, int length);
void tty_read_key_event(tty_t *tty, key_event_t *event); // blocking
int  tty_read(tty_t *tty, char *buffer, int length); // blocking

void tty_log_appender(void *context, const char *str);

#endif
