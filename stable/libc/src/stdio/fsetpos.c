#include "../libc_internal.h"

/**
 * @brief Sets the file position and state for a stream.
 *
 * This function sets the file position indicator and the internal state of
 * the `stream` to the value stored in the `fpos_t` object pointed to by `pos`.
 * The `pos` value must have been obtained by a previous call to `fgetpos`
 * on the same stream.
 *
 * @param stream The `FILE` stream.
 * @param pos Pointer to an `fpos_t` object containing the desired position.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int fsetpos(FILE *stream, const fpos_t *pos) {
    if (!stream || !pos) {
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
    off_t result = lseek(stream->fd, (off_t)*pos, SEEK_SET);
    if (result == (off_t)-1) {
        // lseek failed, errno should be set
        return -1;
    }

    return 0;
}