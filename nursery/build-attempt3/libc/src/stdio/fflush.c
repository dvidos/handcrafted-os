#include "../libc_internal.h"

/**
 * @brief Flushes an output stream.
 *
 * This function forces any buffered output data for the specified output `stream`
 * to be written to the underlying file or device.
 *
 * @param stream The `FILE` stream to flush. If NULL, all open output streams are flushed.
 * @return 0 on success, or `EOF` on error.
 *
 * @implNote
 * This function interacts with the stream's internal buffering. It needs to:
 * 1. Check if `stream` is an output stream and has buffered data.
 * 2. Write the buffered data to the underlying file descriptor (e.g., via `write()` system call).
 * 3. Reset the buffer state.
 */
int fflush(FILE *stream) {
    // TODO: Implement fflush for your operating system.
    // This involves writing buffered data to the underlying file.
    (void)stream; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return EOF;
}