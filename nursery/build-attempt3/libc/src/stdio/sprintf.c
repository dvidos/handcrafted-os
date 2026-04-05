#include "libc_internal.h"

/**
 * @brief Prints formatted output to a string.
 *
 * This function writes the formatted output to the character array `str`.
 * A null terminator is automatically appended at the end of the string.
 *
 * @param str The character array to write the formatted output to.
 * @param format The format string.
 * @param ... Additional arguments corresponding to format specifiers.
 * @return On success, the total number of characters written (excluding the
 *         null terminator) is returned. On error, a negative value is returned.
 *
 * @implNote
 * This function formats output to memory. It needs to handle:
 * 1. Parsing `format` and accessing variadic arguments.
 * 2. Writing characters to `str` and managing the buffer bounds.
 * 3. Appending a null terminator.
 * Crucially, it does not perform any bounds checking, making `snprintf` generally safer.
 */
int sprintf(char *str, const char *format, ...) {
    // TODO: Implement sprintf for your operating system.
    // This involves parsing format strings and writing to a character array.
    (void)str;    // Suppress unused parameter warning
    (void)format; // Suppress unused parameter warning
    // va_list args;
    // va_start(args, format);
    // int ret = vsprintf(str, format, args);
    // va_end(args);
    // return ret;
    errno = ENOSYS; // Function not implemented
    return -1;
}