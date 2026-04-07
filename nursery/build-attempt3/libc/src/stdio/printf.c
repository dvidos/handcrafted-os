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
    char tmp[128];
    int len;

    va_list args;
    va_start(args, format);
    int total = vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);

    if (total < 0)
        return total;
    if ((unsigned)total < sizeof(tmp))
        return write(STDOUT_FILENO, tmp, strlen(tmp));

    // we need more than our stack
    char *big = malloc(total + 1);
    if (big == 0) return -1;

    va_start(args, format);
    vsnprintf(big, total + 1, format, args);
    va_end(args);

    len = write(STDOUT_FILENO, big, total);
    free(big);

    return len;
}