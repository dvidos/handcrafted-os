#include "../libc_internal.h"

/**
 * @brief Prints formatted output to `stdout`.
 *
 * This function writes the C string pointed to by `format` to the standard
 * output stream (`stdout`). If `format` includes format specifiers (e.g., %d, %s),
 * the additional arguments following `format` are formatted and inserted into
 * the resulting string.
 *
 * @param format The format string, composed of zero or more directives.
 * @param ... Additional arguments corresponding to format specifiers in `format`.
 * @return On success, the total number of characters written is returned.
 *         On error, a negative value is returned.
 */
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stdout, format, args);
    va_end(args);
    return ret;
}