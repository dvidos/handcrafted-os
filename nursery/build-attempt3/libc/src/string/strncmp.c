#include "libc_internal.h"

/**
 * @brief Compares two strings up to a specified number of characters.
 *
 * This function compares at most `n` characters from the null-terminated strings
 * `s1` and `s2` lexicographically.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @param n The maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero if `s1` is found,
 *         respectively, to be less than, to match, or be greater than `s2`.
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0; // n characters compared and equal
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}