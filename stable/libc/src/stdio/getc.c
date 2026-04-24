#include "../libc_internal.h"

/**
 * @brief Reads a character from a specified stream (macro variant).
 *
 * This function is often implemented as a macro that expands to `fgetc` or a
 * direct read from the stream's buffer for performance. It reads the next character
 * from the specified input `stream`.
 *
 * @param stream The input stream to read from.
 * @return On success, the character read (as an `int`) is returned. On end-of-file
 *         or error, `EOF` is returned.
 *
 * @implNote
 * If implemented as a function, its behavior is identical to `fgetc`. If as a macro,
 * it avoids function call overhead but may re-evaluate its arguments.
 */
// int getc(FILE *stream) {
//     // TODO: Implement getc for your operating system.
//     // This can be identical to fgetc, or a macro.
//     return fgetc(stream);
// }