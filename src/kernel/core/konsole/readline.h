#ifndef _READLINE_H
#define _READLINE_H

void init_readline(char *prompt);
void readline_add_keyword(char *keyword);
void readline_add_history(char *keyword);
char *readline();

#endif
