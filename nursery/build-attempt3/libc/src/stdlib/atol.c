#include "../libc_internal.h"
#include <string.h> // For strtol
#include <errno.h>  // For errno

/**
 * @brief Converts a string to a long integer.
 *
 * This function converts the initial portion of the string pointed to by `str`
 * to a `long int` representation. It is essentially a wrapper around `strtol`.
 *
 * @param str The string to convert.
 * @return The converted `long int` value.
 *
 * @implNote
 * This is a simple wrapper. It usually calls `strtol` with base 10 and discards
 * the `endptr` argument.
 */
long atol(const char *str) {
    // TODO: Implement atol for your operating system.
    // This is a wrapper around strtol.
    return strtol(str, NULL, 10);
}