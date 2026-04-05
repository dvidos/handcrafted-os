#include "libc_internal.h"
#include <string.h> // For strtol
#include <errno.h>  // For errno

/**
 * @brief Converts a string to an integer.
 *
 * This function converts the initial portion of the string pointed to by `str`
 * to an `int` representation. It is essentially a wrapper around `strtol`.
 *
 * @param str The string to convert.
 * @return The converted `int` value.
 *
 * @implNote
 * This is a simple wrapper. It usually calls `strtol` with base 10 and discards
 * the `endptr` argument.
 */
int atoi(const char *str) {
    // TODO: Implement atoi for your operating system.
    // This is a wrapper around strtol.
    return (int)strtol(str, NULL, 10);
}