#include "../libc_internal.h"

/**
 * @brief Prints formatted output to a string with a specified size limit, using a `va_list` argument.
 *
 * This function is equivalent to `snprintf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param str The character array to write the formatted output to.
 * @param size The maximum number of characters to write, including the null terminator.
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters that would have been written
 *         if `size` had been sufficiently large (excluding the null terminator)
 *         is returned. On error, a negative value is returned.
 *
 * @implNote
 * This is the core engine for `snprintf`. It performs the actual parsing,
 * formatting, and writing safely to the character array `str` with bounds checking.
 * Other `*snprintf` functions often wrap around this function.
 */
// int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
//     // TODO: Implement vsnprintf for your operating system.
//     // This is the core variadic printf function writing safely to a character array.
//     (void)str;    // Suppress unused parameter warning
//     (void)size;   // Suppress unused parameter warning
//     (void)format; // Suppress unused parameter warning
//     (void)ap;     // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }