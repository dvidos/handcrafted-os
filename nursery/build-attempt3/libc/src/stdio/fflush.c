#include "../libc_internal.h"

/**
 * @brief Flushes an output stream.
 *
 * This function forces any buffered output data for the specified output `stream`
 * to be written to the underlying file or device.
 *
 * @param stream The `FILE` stream to flush. If NULL, all open output streams are flushed.
 * @return 0 on success, or `EOF` on error.
 */
int fflush(FILE *stream) {
    if (!stream) {
        // If stream is NULL, behavior is undefined for a single stream.
        // POSIX allows flushing all streams, but C standard says undefined.
        // For now, return error.
        errno = EBADF;
        return EOF;
    }

    // Only flush if in write mode and there's data in the buffer
    if ((stream->flags & _IO_WRITE) && stream->pos > 0) {
        ssize_t written_bytes = write(stream->fd, stream->buffer, stream->pos);
        if (written_bytes != stream->pos) {
            stream->flags |= _IO_ERROR; // Set error flag
            // errno should be set by the underlying write() call
            return EOF;
        }
        stream->pos = 0; // Reset buffer position after flushing
    }
    return 0; // Success
}
