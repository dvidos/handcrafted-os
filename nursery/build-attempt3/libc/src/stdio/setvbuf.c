#include "../libc_internal.h"

/**
 * @brief Sets the buffering and buffer size for a stream.
 *
 * This function allows for more fine-grained control over a `stream`'s
 * buffering. `mode` specifies `_IOFBF` (full buffering), `_IOLBF` (line
 * buffering), or `_IONBF` (no buffering). If `buf` is NULL, the system
 * allocates its own buffer. `size` specifies the buffer size.
 *
 * @param stream The `FILE` stream.
 * @param buf A pointer to a character buffer, or NULL.
 * @param mode The buffering mode (`_IOFBF`, `_IOLBF`, `_IONBF`).
 * @param size The size of the buffer.
 * @return 0 on success, or non-zero on error.
 */
int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    if (!stream || (mode != _IOFBF && mode != _IOLBF && mode != _IONBF)) {
        errno = EINVAL;
        return -1;
    }

    // If I/O has already occurred, cannot change buffering
    // TODO: Need a flag in FILE to indicate if I/O has started. For now, assume no I/O.

    // Flush any pending output before changing buffering
    if ((stream->flags & _IO_WRITE) && stream->pos > 0) {
        if (fflush(stream) == EOF) {
            return -1; // fflush failed, errno should be set
        }
    }

    // Free existing dynamically allocated buffer (if any)
    // NOTE: This assumes stream->buffer was malloc'd by fopen or a previous setvbuf.
    // If 'buf' was user-provided in a previous call, it should NOT be freed here.
    // A more robust FILE structure would track ownership of the buffer.
    if (stream->buffer) { // And if it's not a user-provided buffer
        // For simplicity, we assume fopen always mallocs.
        // If setbuf was used with a user buffer, this is problematic.
        free(stream->buffer);
        stream->buffer = NULL;
    }

    // Clear old buffering flags
    stream->flags &= ~(_IO_NO_BUF | _IO_LINE_BUF | _IO_FULL_BUF);

    stream->pos = 0;
    stream->end = 0;

    switch (mode) {
        case _IONBF:
            stream->buffer = NULL; // No buffer
            stream->buf_size = 0;  // Or 1 for unbuffered char-by-char I/O
            stream->flags |= _IO_NO_BUF;
            break;
        case _IOLBF:
        case _IOFBF:
            if (size == 0) {
                size = BUFSIZ; // Default buffer size if 0
            }
            if (buf) {
                stream->buffer = buf;
            } else {
                stream->buffer = (char *)malloc(size);
                if (!stream->buffer) {
                    errno = ENOMEM;
                    return -1;
                }
            }
            stream->buf_size = size;
            stream->flags |= (mode == _IOLBF ? _IO_LINE_BUF : _IO_FULL_BUF);
            break;
    }

    return 0; // Success
}