#include "../libc_internal.h"

/**
 * @brief Locates the last occurrence of a character in a string.
 *
 * This function searches for the last occurrence of the character `c`
 * (converted to `char`) in the string `s`. The null terminator is
 * considered part of the string.
 *
 * @param s The string to search.
 * @param c The character to locate.
 * @return A pointer to the last occurrence of `c` in `s`, or NULL if `c`
 *         is not found.
 */
char *strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last_occurrence = NULL;

    while (1) {
        if (*s == ch) {
            last_occurrence = s;
        }
        if (*s == '\0') {
            break;
        }
        s++;
    }
    return (char *)last_occurrence;
}