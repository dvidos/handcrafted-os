#include "libc_internal.h"

/**
 * @brief Writes a character to `stdout`.
 *
 * This function writes the character `c` (converted to an `unsigned char`)
 * to the standard output stream (`stdout`).
 *
 * @param c The character to write, cast to an `int`.
 * @return On success, the character written is returned. On error, `EOF` is returned.
 *
 * @implNote
 * This is a basic character output function. It typically uses `fputc`
 * internally or directly interacts with `stdout`'s buffer.
 */
int putchar(int c) {
    // TODO: Implement putchar for your operating system.
    // This involves writing a single character to stdout.
    (void)c; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return EOF;
}