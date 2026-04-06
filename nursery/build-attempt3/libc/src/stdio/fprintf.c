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
 *
 * @implNote
 * This function shares much of its implementation with `printf`.
 * It needs to direct the formatted output to the `stream`'s internal
 * buffering mechanism and ultimately to the underlying file descriptor.
 */
int fprintf(FILE *stream, const char *format, ...) {
    // TODO: Implement fprintf for your operating system.
    // This involves parsing format strings and writing to the specified stream.
    (void)stream; // Suppress unused parameter warning
    (void)format; // Suppress unused parameter warning
    // va_list args;
    // va_start(args, format);
    // int ret = vfprintf(stream, format, args);
    // va_end(args);
    // return ret;
    errno = ENOSYS; // Function not implemented
    return -1;
}