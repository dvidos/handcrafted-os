#include "../libc_internal.h"
#include <string.h> // For tolower (or implement it directly if not available)

/**
 * @brief Compares two strings case-insensitively. (Non-standard but very common)
 *
 * This function compares the null-terminated strings `s1` and `s2` lexicographically,
 * ignoring differences in case.
 *
 * @param s1 The first string.
 * @param s2 The second string.
 * @return An integer less than, equal to, or greater than zero if `s1` is found,
 *         respectively, to be less than, to match, or be greater than `s2`,
 *         ignoring case.
 */
int strcasecmp(const char *s1, const char *s2) {
    // TODO: Implement strcasecmp.
    // This function is non-standard but very common.
    // It typically uses tolower() on each character before comparison.
    (void)s1; // Suppress unused parameter warning
    (void)s2; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0;
}