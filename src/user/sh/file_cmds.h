#ifndef _FILE_CMDS_H
#define _FILE_CMDS_H

#include <ctypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errors.h>

struct built_in_info {
    char *name;
    void (*func)(int argc, char *argv[]);
};

extern struct built_in_info built_ins[];



#endif

