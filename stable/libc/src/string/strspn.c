#include "../libc_internal.h"
#include <string.h> // For internal dependencies
#include <stddef.h> // For size_t

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    if (!s || !accept) {
        return 0;
    }

    while (*s != '\0') {
        const char *accept_ptr = accept;
        int found = 0;
        while (*accept_ptr != '\0') {
            if (*s == *accept_ptr) {
                found = 1;
                break;
            }
            accept_ptr++;
        }
        if (found) {
            count++;
            s++;
        } else {
            break; // Character not in accept set
        }
    }
    return count;
}
