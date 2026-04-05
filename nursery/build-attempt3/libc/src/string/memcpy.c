#include "libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Copies bytes from source to destination.
 *
 * This function copies `n` bytes from the memory area pointed to by `src`
 * to the memory area pointed to by `dest`.
 * The behavior is undefined if the source and destination memory areas overlap.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n The number of bytes to copy.
 * @return A pointer to the destination memory area `dest`.
 */
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}