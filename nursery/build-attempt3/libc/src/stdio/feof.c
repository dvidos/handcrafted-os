#include "../libc_internal.h"

/**
 * @brief Checks the end-of-file indicator for a stream.
 *
 * This function tests the end-of-file indicator for the `stream`.
 *
 * @param stream The `FILE` stream.
 * @return Non-zero if the end-of-file indicator is set, 0 otherwise.
 */
int feof(FILE *stream) {
    if (!stream) {
        errno = EINVAL; // Invalid argument
        return 0;       // Standard behavior undefined, return 0 is safer
    }
    return (stream->flags & _IO_EOF) ? 1 : 0;
}