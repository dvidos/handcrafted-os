#ifndef _TTY_H
#define _TTY_H

#include "../drivers/keyboard.h"

typedef struct tty tty_t;

// for kernel management
void init_tty_manager(int num_of_ttys, int lines_scroll_capacity);
tty_t *tty_manager_get_device(int number);




// called by processes
void tty_read_key(tty_t *tty, key_event_t *event);
void tty_write(tty_t *tty, char *buffer, int len);
void tty_set_color(tty_t *tty, int color);
void tty_clear(tty_t *tty);


#endif
