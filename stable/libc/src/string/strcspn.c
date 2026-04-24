#include "../libc_internal.h"
#include <string.h> // For internal dependencies
#include <stddef.h> // For size_t, NULL

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    if (!s || !reject) {
        return 0;
    }

    while (*s != '\0') {
        const char *reject_ptr = reject;
        int found_in_reject = 0;
        while (*reject_ptr != '\0') {
            if (*s == *reject_ptr) {
                found_in_reject = 1;
                break; // Character is in reject set, so stop counting
            }
            reject_ptr++;
        }
        if (found_in_reject) {
            break; // Character is in reject set, segment ends here
        } else {
            count++;
            s++;
        }
    }
    return count;
}