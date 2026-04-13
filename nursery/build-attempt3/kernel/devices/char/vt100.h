#include "../../include/uapi/errors.h"


typedef struct vt100 vt100_t;

struct vt100_ops {

    // accepts ascii data, or escaped commands
    error_t puts(char *str);
    error_t putc(int c);

    // blocking or not, depending on flags
    // ascii or escaped keys
    char *gets();
    int getc();

    void getopt(int opt, int *value);
    void setopt(int opt, int value);
};

