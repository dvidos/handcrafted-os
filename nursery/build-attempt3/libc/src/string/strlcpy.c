#include "../libc_internal.h"
#include <string.h> // For strlen
#include <stddef.h> // For size_t

size_t strlcpy(char *dst, const char *src, size_t dsize) {
    const char *s = src;
    char *d = dst;
    size_t n = dsize;
    size_t src_len = strlen(src); // Calculate source length once

    // Handle zero size case first
    if (n == 0) {
        return src_len; // Return the full source length
    }

    // Copy up to n-1 characters from src to dst
    while (*s != '\0' && --n != 0) {
        *d++ = *s++;
    }

    // Null-terminate dst
    *d = '\0';

    return src_len; // Always return the length of the source string
}