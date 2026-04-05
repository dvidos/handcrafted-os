#include "libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Copies bytes from source to destination, handling overlapping memory.
 *
 * This function copies `n` bytes from the memory area pointed to by `src`
 * to the memory area pointed to by `dest`. Unlike `memcpy`, `memmove`
 * correctly handles cases where the source and destination memory areas overlap.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n The number of bytes to copy.
 * @return A pointer to the destination memory area `dest`.
 */
void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        // Copy from left to right
        while (n--) {
            *d++ = *s++;
        }
    } else {
        // Copy from right to left to handle overlap
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    }
    return dest;
}