#include "libc_internal.h"
#include <string.h> // For strtoll
#include <errno.h>  // For errno

/**
 * @brief Converts a string to a long long integer.
 *
 * This function converts the initial portion of the string pointed to by `str`
 * to a `long long int` representation. It is essentially a wrapper around `strtoll`.
 *
 * @param str The string to convert.
 * @return The converted `long long int` value.
 *
 * @implNote
 * This is a simple wrapper. It usually calls `strtoll` with base 10 and discards
 * the `endptr` argument.
 */
long long atoll(const char *str) {
    // TODO: Implement atoll for your operating system.
    // This is a wrapper around strtoll.
    return strtoll(str, NULL, 10);
}