#include "libc_internal.h"
#include <string.h> // For strlen

/**
 * @brief Appends a specified number of characters from one string to another.
 *
 * This function appends at most `n` characters from the string pointed to by `src`
 * to the end of the null-terminated string pointed to by `dest`. The first character
 * of `src` overwrites the null terminator of `dest`. A null terminator is
 * always appended to the result if `n` allows.
 *
 * @param dest Pointer to the destination buffer.
 * @param src Pointer to the source string.
 * @param n The maximum number of characters to append from `src`.
 * @return A pointer to the destination buffer `dest`.
 */
char *strncat(char *dest, const char *src, size_t n) {
    char *original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while (n > 0 && (*dest++ = *src++) != '\0') {
        n--;
    }
    *dest = '\0'; // Ensure null-termination
    return original_dest;
}