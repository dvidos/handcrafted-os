#include "../libc_internal.h"
#include <string.h> // For strtod
#include <errno.h>  // For errno

/**
 * @brief Converts a string to a double-precision floating-point number.
 *
 * This function converts the initial portion of the string pointed to by `str`
 * to a `double` representation. It is essentially a wrapper around `strtod`.
 *
 * @param str The string to convert.
 * @return The converted `double` value.
 *
 * @implNote
 * This is a simple wrapper. It usually calls `strtod` and discards the `endptr` argument.
 */
double atof(const char *str) {
    // TODO: Implement atof for your operating system.
    // This is a wrapper around strtod.
    return strtod(str, NULL);
}