#include "../libc_internal.h"
#include <stdlib.h> // For strtol

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}