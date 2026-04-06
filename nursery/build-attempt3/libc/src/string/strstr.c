#include "../libc_internal.h"
#include <stddef.h> // For size_t

/**
 * @brief Locates the first occurrence of a substring.
 *
 * This function finds the first occurrence of the null-terminated substring
 * `needle` in the null-terminated string `haystack`.
 *
 * @param haystack The string to search within.
 * @param needle The substring to search for.
 * @return A pointer to the first occurrence of `needle` in `haystack`,
 *         or NULL if `needle` is not found. If `needle` is an empty string,
 *         `haystack` is returned.
 *
 * @implNote
 * This function involves searching algorithms. A naive implementation
 * involves nested loops, but more optimized algorithms (e.g., Boyer-Moore,
 * Knuth-Morris-Pratt) can be used for better performance, especially
 * with long strings and needles.
 */
char *strstr(const char *haystack, const char *needle) {
    // TODO: Implement strstr for your operating system.
    // This is a complex string searching algorithm.
    (void)haystack; // Suppress unused parameter warning
    (void)needle;   // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}