#include "libc_internal.h"
#include <string.h> // For memcpy, strlen

/**
 * @brief Copies a specified number of characters from one string to another.
 *
 * This function copies at most `n` characters from the string pointed to by `src`
 * to the buffer pointed to by `dest`. If `src` is shorter than `n`, `dest` is
 * padded with null bytes until `n` characters have been written. If `src` is
 * `n` characters or longer, `dest` will not be null-terminated.
 * The behavior is undefined if the source and destination buffers overlap.
 *
 * @param dest Pointer to the destination buffer.
 * @param src Pointer to the source string.
 * @param n The maximum number of characters to copy.
 * @return A pointer to the destination buffer `dest`.
 */
char *strncpy(char *dest, const char *src, size_t n) {
    char *original_dest = dest;
    while (n > 0 && (*dest++ = *src++) != '\0') {
        n--;
    }
    while (n > 0) { // Pad with nulls if src was shorter than n
        *dest++ = '\0';
        n--;
    }
    return original_dest;
}