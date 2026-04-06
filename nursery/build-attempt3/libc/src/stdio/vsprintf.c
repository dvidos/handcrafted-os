#include "../libc_internal.h"

/**
 * @brief Prints formatted output to a string using a `va_list` argument.
 *
 * This function is equivalent to `sprintf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param str The character array to write the formatted output to.
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters written (excluding the
 *         null terminator) is returned. On error, a negative value is returned.
 *
 * @implNote
 * This is the core engine for `sprintf`. It performs the actual parsing,
 * formatting, and writing to the character array `str`. Other `*sprintf` functions often
 * wrap around this function. Does not perform bounds checking.
 */
int vsprintf(char *str, const char *format, va_list ap) {
    // TODO: Implement vsprintf for your operating system.
    // This is the core variadic printf function writing to a character array.
    (void)str;    // Suppress unused parameter warning
    (void)format; // Suppress unused parameter warning
    (void)ap;     // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}