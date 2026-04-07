#include "../libc_internal.h"

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
 */
int sprintf(char *str, const char *format, ...) {
    va_list args;
    
    va_start(args, format);
    int total = vsnprintf(str, SIZE_MAX, format, args);
    va_end(args);

    return total;
}