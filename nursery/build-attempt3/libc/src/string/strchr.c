#include "libc_internal.h"

/**
 * @brief Locates the first occurrence of a character in a string.
 *
 * This function searches for the first occurrence of the character `c`
 * (converted to `char`) in the string `s`. The null terminator is
 * considered part of the string.
 *
 * @param s The string to search.
 * @param c The character to locate.
 * @return A pointer to the first occurrence of `c` in `s`, or NULL if `c`
 *         is not found.
 */
char *strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) {
            return (char *)s;
        }
        s++;
    }
    if (ch == '\0') { // Check for null terminator itself
        return (char *)s;
    }
    return NULL;
}