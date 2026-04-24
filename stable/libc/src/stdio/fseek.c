#include "../libc_internal.h"

/**
 * @brief Sets the file position indicator for a stream.
 *
 * This function sets the file position indicator for the `stream` to a new
 * position. The new position is determined by `offset` and `whence`:
 * - `SEEK_SET`: `offset` bytes from the beginning of the file.
 * - `SEEK_CUR`: `offset` bytes from the current position.
 * - `SEEK_END`: `offset` bytes from the end of the file.
 *
 * @param stream The `FILE` stream.
 * @param offset The offset to apply.
 * @param whence The starting point for the offset (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`).
 * @return 0 on success, or -1 on error with `errno` set.
 */
int fseek(FILE *stream, off_t offset, int whence) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    // Flush any pending output
    if ((stream->flags & _IO_WRITE) && stream->pos > 0) {
        if (fflush(stream) == EOF) {
            return -1; // fflush failed, errno should be set
        }
    }

    // Discard any buffered input
    stream->pos = 0;
    stream->end = 0;

    // Clear EOF and error flags
    stream->flags &= ~(_IO_EOF | _IO_ERROR);

    // Set the underlying file descriptor's position
    off_t result = lseek(stream->fd, offset, whence);
    if (result == (off_t)-1) {
        // lseek failed, errno should be set
        return -1;
    }

    return 0;
}