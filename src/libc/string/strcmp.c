#include <string.h>

int strcmp(const char* a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return (*a - *b);
        a++;
        b++;
    }
    if (*a == '\0')
        return 1;
    if (*b == '\0')
        return -1;
    return 0;
}
