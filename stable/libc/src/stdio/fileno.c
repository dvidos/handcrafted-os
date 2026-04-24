#include "../libc_internal.h"

/**
 * @brief Gets the file descriptor associated with a stream.
 *
 * This function returns the integer file descriptor associated with the
 * `FILE` stream `stream`.
 *
 * @param stream The `FILE` stream.
 * @return The file descriptor associated with `stream`.
 */
int fileno(FILE *stream) {
    if (!stream) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }
    return stream->fd;
}
