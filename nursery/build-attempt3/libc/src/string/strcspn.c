#include "../libc_internal.h"
#include <string.h> // For strchr

/**
 * @brief Calculates the length of the initial segment of a string that consists
 *        entirely of characters *not* found in another string.
 *
 * This function calculates the length of the maximum initial segment of the
 * string `s` that consists solely of characters *not* found in the string `reject`.
 *
 * @param s The string to search.
 * @param reject The string containing characters to reject.
 * @return The length of the segment.
 */
size_t strcspn(const char *s, const char *reject) {
    // TODO: Implement strcspn.
    // This typically involves iterating through 's' and for each char, checking
    // if it exists in 'reject'.
    (void)s;      // Suppress unused parameter warning
    (void)reject; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0;
}