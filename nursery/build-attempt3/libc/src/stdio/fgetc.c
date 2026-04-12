#include "../libc_internal.h"

/**
 * @brief Reads a character from a specified stream.
 *
 * This function reads the next character from the specified input `stream`.
 *
 * @param stream The input stream to read from.
 * @return On success, the character read (as an `int`) is returned. On end-of-file
 *         or error, `EOF` is returned.
 */
int fgetc(FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    // Check for a pushed-back character first
    if (stream->has_ungetc_char) {
        stream->has_ungetc_char = false;
        return stream->ungetc_char;
    }

    if (!(stream->flags & _IO_READ)) {
        errno = EBADF; // Stream not open for reading
        stream->flags |= _IO_ERROR;
        return EOF;
    }

    // If buffer is empty or depleted, refill it
    if (stream->pos >= stream->end) {
        // If already at EOF, return EOF
        if (stream->flags & _IO_EOF) {
            return EOF;
        }

        ssize_t bytes_from_fd = read(stream->fd, stream->buffer, stream->buf_size);
        // syslog_debug("fgetc(): read returned %d, whole buffer is '%s'", bytes_from_fd, stream->buffer);
        if (bytes_from_fd == 0) {
            stream->flags |= _IO_EOF; // Set EOF flag
            return EOF; // End of file
        } else if (bytes_from_fd < 0) {
            stream->flags |= _IO_ERROR; // Set error flag
            // errno is set by the underlying read()
            return EOF; // Read error
        }
        stream->end = bytes_from_fd;
        stream->pos = 0;
    }

    // Return character from buffer
    return (int)(unsigned char)stream->buffer[stream->pos++];
}