#include "../libc_internal.h"

/**
 * @brief Prints formatted output to `stdout` using a `va_list` argument.
 *
 * This function is equivalent to `printf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters written is returned.
 *         On error, a negative value is returned.
 *
 * @implNote
 * This is the core engine for `printf`. It performs the actual parsing,
 * formatting, and writing to `stdout`. Other `*printf` functions often
 * wrap around this function.
 */
int vprintf(const char *format, va_list ap) {
    char tmp[128];
    int len;

    int total = vsnprintf(tmp, sizeof(tmp), format, ap);
    if (total < 0)
        return total;
    if ((unsigned)total < sizeof(tmp))
        return write(STDOUT_FILENO, tmp, strlen(tmp));

    // we need more than our stack
    char *big = malloc(total + 1);
    if (big == 0) return -1;

    vsnprintf(big, total + 1, format, ap);

    len = write(STDOUT_FILENO, big, total);
    free(big);

    return len;
}