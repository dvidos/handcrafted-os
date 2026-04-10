#include "../libc_internal.h"

/**
 * @brief Writes a character to `stdout`.
 *
 * This function writes the character `c` (converted to an `unsigned char`)
 * to the standard output stream (`stdout`).
 *
 * @param c The character to write, cast to an `int`.
 * @return On success, the character written is returned. On error, `EOF` is returned.
 */
int putchar(int c) {
    return fputc(c, stdout);
}
