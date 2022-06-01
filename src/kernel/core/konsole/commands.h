#ifndef _COMMANDS_H
#define _COMMANDS_H

struct command {
    char *cmd;
    char *descr;
    int (*func_ptr)();
};

#endif