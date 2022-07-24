#include <string.h>

void strcpy(char *target, char *source) {
    while (*source != '\0') {
        *target++ = *source++;
    }
    *target = *source; // final null char
}

