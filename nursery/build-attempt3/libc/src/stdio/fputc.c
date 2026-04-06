#include "../libc_internal.h"

/**
 * @brief Writes a character to a specified stream.
 *
 * This function writes the character `c` (converted to an `unsigned char`)
 * to the specified output `stream`.
 *
 * @param c The character to write, cast to an `int`.
 * @param stream The output stream to write to.
 * @return On success, the character written is returned. On error, `EOF` is returned.
 *
 * @implNote
 * This is a fundamental character output function. It manages the `stream`'s
 * buffering (full, line, or unbuffered) and eventually calls the underlying
 * system call (e.g., `write`) to output the character.
 */
int fputc(int c, FILE *stream) {
    // TODO: Implement fputc for your operating system.
    // This involves writing a single character to the specified stream, managing buffering.
    (void)c;      // Suppress unused parameter warning
    (void)stream; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return EOF;
}