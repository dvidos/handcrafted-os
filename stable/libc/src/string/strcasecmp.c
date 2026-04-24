#include "../libc_internal.h"
#include <string.h> // For general string functions if needed
#include <ctype.h>  // For tolower

int strcasecmp(const char *s1, const char *s2) {
    // According to POSIX, behavior is undefined if s1 or s2 is a null pointer.
    // We proceed assuming valid non-NULL inputs as per common libc implementations.

    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (*p1 != '\0' && *p2 != '\0') {
        int diff = tolower(*p1) - tolower(*p2);
        if (diff != 0) {
            return diff;
        }
        p1++;
        p2++;
    }

    // If one string ended, compare the null terminators (or lack thereof)
    return tolower(*p1) - tolower(*p2);
}