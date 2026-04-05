#include "libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Computes the length of a string.
 *
 * This function computes the length of the null-terminated string `s`,
 * not including the null terminator.
 *
 * @param s The string.
 * @return The number of characters in `s` before the null terminator.
 */
size_t strlen(const char *s) {
    size_t length = 0;
    while (*s++ != '\0') {
        length++;
    }
    return length;
}