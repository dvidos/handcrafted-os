#include "../libc_internal.h"

/**
 * @brief Gets the current file position and state.
 *
 * This function stores the current file position indicator and the internal
 * state of the `stream` into the `fpos_t` object pointed to by `pos`.
 * This object can then be used by `fsetpos` to restore the stream's state.
 *
 * @param stream The `FILE` stream.
 * @param pos Pointer to an `fpos_t` object to store the position.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int fgetpos(FILE *stream, fpos_t *pos) {
    if (!stream || !pos) {
        errno = EINVAL;
        return -1;
    }

    // Flush output buffer if the stream is write-enabled and has buffered data
    // This is important to get an accurate file offset for writing streams
    if ((stream->flags & _IO_WRITE) && stream->pos > 0) {
        if (fflush(stream) == EOF) {
            return -1; // fflush failed, errno should be set
        }
    }

    off_t current_offset = lseek(stream->fd, 0, SEEK_CUR);
    if (current_offset == (off_t)-1) {
        // lseek failed, errno should be set
        return -1;
    }

    // Adjust for buffered input data
    if (stream->flags & _IO_READ) {
        current_offset -= (stream->end - stream->pos);
    }

    *pos = (fpos_t)current_offset;
    return 0;
}