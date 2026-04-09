#include "../libc_internal.h"

/**
 * @brief Prints formatted output to a specified stream.
 *
 * This function is identical to `printf` but writes the output to the
 * specified output `stream` instead of `stdout`.
 *
 * @param stream The output stream to write to.
 * @param format The format string.
 * @param ... Additional arguments corresponding to format specifiers.
 * @return On success, the total number of characters written is returned.
 *         On error, a negative value is returned.
 */
int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    return ret;
}
