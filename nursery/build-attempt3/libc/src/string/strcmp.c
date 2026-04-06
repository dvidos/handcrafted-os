#include "../libc_internal.h"

/**
 * @brief Compares two strings.
 *
 * This function compares the null-terminated strings `s1` and `s2` lexicographically.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return An integer less than, equal to, or greater than zero if `s1` is found,
 *         respectively, to be less than, to match, or be greater than `s2`.
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}