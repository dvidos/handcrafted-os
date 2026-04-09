#include "../libc_internal.h"

/**
 * @brief Gets the current file position indicator for a stream.
 *
 * This function returns the current value of the file position indicator
 * for the `stream`. This value represents the offset from the beginning
 * of the file, in bytes.
 *
 * @param stream The `FILE` stream.
 * @return On success, the current file position is returned. On error, -1L
 *         is returned, and `errno` is set.
 */
long ftell(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return -1L;
    }

    // Flush output buffer if the stream is write-enabled and has buffered data
    if ((stream->flags & _IO_WRITE) && stream->pos > 0) {
        if (fflush(stream) == EOF) {
            return -1L; // fflush failed, errno should be set
        }
    }

    off_t current_offset = lseek(stream->fd, 0, SEEK_CUR);
    if (current_offset == (off_t)-1) {
        // lseek failed, errno should be set
        return -1L;
    }

    // Adjust for buffered input data
    if (stream->flags & _IO_READ) {
        current_offset -= (stream->end - stream->pos);
    }

    return (long)current_offset;
}