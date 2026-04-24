#ifndef _CMD_LINE_H
#define _CMD_LINE_H

#include <ctypes.h>
#include "buffer.h"

bool edit_command_line(char first_char, char *cmd_line);
void execute_command_line(char *cmd_line, buffer_t *buffer, bool *should_quit);



#endif
