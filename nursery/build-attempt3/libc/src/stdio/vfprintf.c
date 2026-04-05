#include "libc_internal.h"

/**
 * @brief Prints formatted output to a specified stream using a `va_list` argument.
 *
 * This function is equivalent to `fprintf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param stream The output stream to write to.
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters written is returned.
 *         On error, a negative value is returned.
 *
 * @implNote
 * This is the core engine for `fprintf`. It performs the actual parsing,
 * formatting, and writing to the specified `stream`. Other `*fprintf` functions often
 * wrap around this function.
 */
int vfprintf(FILE *stream, const char *format, va_list ap) {
    // TODO: Implement vfprintf for your operating system.
    // This is the core variadic printf function writing to a specific stream.
    (void)stream; // Suppress unused parameter warning
    (void)format; // Suppress unused parameter warning
    (void)ap;     // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}