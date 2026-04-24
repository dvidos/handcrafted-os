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
 */
int ungetc(int c, FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    // Standard guarantees only one character pushback.
    if (stream->has_ungetc_char) {
        errno = EOVERFLOW; // Or other appropriate error
        return EOF;
    }

    // Cannot push back EOF
    if (c == EOF) {
        return EOF;
    }

    stream->ungetc_char = c;
    stream->has_ungetc_char = true;

    // Clear EOF flag, as we are no longer at EOF if a char is pushed back.
    stream->flags &= ~_IO_EOF;

    return c;
}