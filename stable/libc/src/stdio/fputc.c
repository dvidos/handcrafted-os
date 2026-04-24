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
 */
int fputc(int c, FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    if (!(stream->flags & _IO_WRITE)) {
        errno = EBADF; // Stream not open for writing
        stream->flags |= _IO_ERROR;
        return EOF;
    }

    // If buffer is full, flush it
    if (stream->pos >= stream->buf_size) {
        if (fflush(stream) == EOF) {
            return EOF; // Error during flush
        }
    }

    // Place character in buffer
    stream->buffer[stream->pos++] = (unsigned char)c;

    // Handle line buffering (_IOLBF) - flush on newline
    if ((stream->flags & _IO_LINE_BUF) && c == '\n') {
        if (fflush(stream) == EOF) {
            return EOF; // Error during flush
        }
    }

    return (unsigned char)c;
}