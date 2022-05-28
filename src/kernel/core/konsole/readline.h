#ifndef _READLINE_H
#define _READLINE_H

#include "../devices/tty.h"

void init_readline(char *prompt, tty_t *tty);
void readline_add_keyword(char *keyword);
void readline_add_history(char *keyword);
char *readline();

#endif
