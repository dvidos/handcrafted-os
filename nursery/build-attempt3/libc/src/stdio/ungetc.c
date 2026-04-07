#include "../libc_internal.h"

/**
 * @brief Pushes a character back onto an input stream.
 *
 * This function pushes the character `c` back onto the input `stream`.
 * The pushed character will be the next character read from the stream.
 * Only one character of pushback is guaranteed.
 *
 * @param c The character to push back.
 * @param stream The input stream.
 * @return On success, the character `c` is returned. On error, `EOF` is returned.
 *
 * @implNote
 * This function typically stores the character `c` in an internal buffer
 * associated with the `stream` such that the next `fgetc` (or similar) call
 * retrieves it. It can be complex to implement correctly with full buffering.
 */
// int ungetc(int c, FILE *stream) {
//     // TODO: Implement ungetc for your operating system.
//     // This involves placing a character back into the stream's buffer.
//     (void)c;      // Suppress unused parameter warning
//     (void)stream; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return EOF;
// }