#ifndef _READLINE_H
#define _READLINE_H


typedef struct readline_info readline_t;

readline_t *init_readline(char *prompt);
void readline_add_keyword(readline_t *rl, char *keyword);
void readline_add_history(readline_t *rl, char *keyword);
char *readline(readline_t *rl);


#endif
