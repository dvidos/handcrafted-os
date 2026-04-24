#include "../libc_internal.h"
#include <string.h> // For strlen
#include <stddef.h> // For size_t

size_t strlcat(char *dst, const char *src, size_t dsize) {
    char *d = dst;
    const char *s = src;
    size_t n = dsize;
    size_t dlen;

    // Find the end of dst and adjust byte limit
    while (n-- != 0 && *d != '\0') {
        d++;
    }
    dlen = d - dst;
    n = dsize - dlen; // Remaining space including null terminator

    if (n == 0) { // No space to copy anything
        return dlen + strlen(s); // Return what would have been the full length
    }

    while (*s != '\0') {
        if (n != 1) { // Leave space for null terminator
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0'; // Null-terminate

    return dlen + (s - src); // Total length that *would* have been, before truncation
}