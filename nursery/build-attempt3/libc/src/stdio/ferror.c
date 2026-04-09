#include "../libc_internal.h"

/**
 * @brief Checks the error indicator for a stream.
 *
 * This function tests the error indicator for the `stream`.
 *
 * @param stream The `FILE` stream.
 * @return Non-zero if the error indicator is set, 0 otherwise.
 */
int ferror(FILE *stream) {
    if (!stream) {
        errno = EINVAL; // Invalid argument
        return 0;       // Standard behavior undefined, return 0 is safer
    }
    return (stream->flags & _IO_ERROR) ? 1 : 0;
}