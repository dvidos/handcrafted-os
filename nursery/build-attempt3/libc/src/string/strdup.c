#include "../libc_internal.h"
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
 */
char *strdup(const char *s) {
    if (s == NULL)
        return NULL;

    int len = strlen(s);
    char *p = malloc(len + 1);
    if (p == NULL)
        return NULL;

    strcpy(p, s);
    return p;
}