#include "../libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Copies a character to a specified number of bytes in memory.
 *
 * This function fills the first `n` bytes of the memory area pointed to by `s`
 * with the constant byte `c`.
 *
 * @param s Pointer to the memory area.
 * @param c The character to fill with, converted to an `unsigned char`.
 * @param n The number of bytes to fill.
 * @return A pointer to the memory area `s`.
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    unsigned char val = (unsigned char)c;
    while (n--) {
        *p++ = val;
    }
    return s;
}