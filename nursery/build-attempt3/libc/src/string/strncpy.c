#include <string.h>


void strncpy(char *target, char *source, size_t target_size) {
    while (*source != '\0' && target_size-- > 1) {
        *target++ = *source++;
    }

    // in any case, we either have the source pointing to the zero terminator
    // or we only have space left for one character in target.
    *target = '\0';
}

