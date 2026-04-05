#include "libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Compares two blocks of memory.
 *
 * This function compares the first `n` bytes of the memory areas `s1` and `s2`.
 *
 * @param s1 Pointer to the first memory area.
 * @param s2 Pointer to the second memory area.
 * @param n The number of bytes to compare.
 * @return An integer less than, equal to, or greater than zero if the first
 *         `n` bytes of `s1` are found, respectively, to be less than, to match,
 *         or be greater than the first `n` bytes of `s2`.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    while (n > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
        n--;
    }
    return 0;
}