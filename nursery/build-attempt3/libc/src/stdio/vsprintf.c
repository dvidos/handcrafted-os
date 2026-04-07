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
 */
int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, SIZE_MAX, format, ap);
}
