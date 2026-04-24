#include "../libc_internal.h"

/**
 * @brief Clears the end-of-file and error indicators for a stream.
 *
 * This function clears the end-of-file and error indicators for the `stream`.
 *
 * @param stream The `FILE` stream.
 */
void clearerr(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return;
    }
    stream->flags &= ~(_IO_EOF | _IO_ERROR);
}