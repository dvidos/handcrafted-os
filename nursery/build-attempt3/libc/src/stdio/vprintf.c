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
// int vprintf(const char *format, va_list ap) {
//     // TODO: Implement vprintf for your operating system.
//     // This is the core variadic printf function writing to stdout.
//     (void)format; // Suppress unused parameter warning
//     (void)ap;     // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }