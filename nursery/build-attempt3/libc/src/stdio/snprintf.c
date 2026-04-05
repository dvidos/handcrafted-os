#include "libc_internal.h"

/**
 * @brief Prints formatted output to a string with a specified size limit.
 *
 * This function writes the formatted output to the character array `str`,
 * but it writes at most `size - 1` characters, ensuring that the buffer
 * does not overflow. A null terminator is always appended unless `size` is 0.
 *
 * @param str The character array to write the formatted output to.
 * @param size The maximum number of characters to write, including the null terminator.
 * @param format The format string.
 * @param ... Additional arguments corresponding to format specifiers.
 * @return On success, the total number of characters that would have been written
 *         if `size` had been sufficiently large (excluding the null terminator)
 *         is returned. On error, a negative value is returned.
 *
 * @implNote
 * This is the safer version of `sprintf`. Its implementation is similar but
 * requires careful bounds checking during character writing to `str`.
 * The return value behavior is important: it indicates how many characters
 * *would have been* written, which can be useful for determining buffer needs.
 */
int snprintf(char *str, size_t size, const char *format, ...) {
    // TODO: Implement snprintf for your operating system.
    // This involves parsing format strings and writing safely to a character array.
    (void)str;    // Suppress unused parameter warning
    (void)size;   // Suppress unused parameter warning
    (void)format; // Suppress unused parameter warning
    // va_list args;
    // va_start(args, format);
    // int ret = vsnprintf(str, size, format, args);
    // va_end(args);
    // return ret;
    errno = ENOSYS; // Function not implemented
    return -1;
}