#include <string.h>

void strcpy(char *target, const char *source) {
    while (*source != '\0') {
        *target++ = *source++;
    }
    *target = *source; // final null char
}

