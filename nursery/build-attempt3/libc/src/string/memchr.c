#include "../libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Returns a pointer to the first occurence of a char in a mempry area.
 *
 * @param s Pointer to the memory area.
 * @param c The character to find, converted to an `unsigned char`.
 * @param n The number of bytes to search.
 * @return A pointer to the first occurence.
 */
void *memchr(const void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;
    while (n--) {
        if (*p == val)
            return p;
        p++;
    }
    return NULL;
}