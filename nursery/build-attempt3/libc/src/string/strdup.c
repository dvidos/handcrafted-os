#include "libc_internal.h"
#include <stdlib.h> // For malloc
#include <string.h> // For strlen, strcpy

/**
 * @brief Duplicates a string. (POSIX extension)
 *
 * This function returns a pointer to a new string which is a duplicate of the
 * string `s`. Memory for the new string is obtained with `malloc`, and can
 * be freed with `free`.
 *
 * @param s The string to duplicate.
 * @return A pointer to the duplicated string, or NULL if insufficient memory
 *         was available. `errno` is set on error.
 *
 * @implNote
 * This function combines `strlen`, `malloc`, and `strcpy`. It's a convenient
 * way to create a dynamically allocated copy of a string.
 */
char *strdup(const char *s) {
    // TODO: Implement strdup for your operating system.
    // This involves allocating memory and copying the string.
    (void)s; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}