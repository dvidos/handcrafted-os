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
 *
 * @implNote
 * This function typically maps to the `lseek(fd, 0, SEEK_CUR)` system call
 * on the stream's underlying file descriptor. It must also account for any
 * buffered data that has been read but not yet consumed by the user.
 */
// long ftell(FILE *stream) {
//     // TODO: Implement ftell for your operating system.
//     // This involves querying the underlying file descriptor's position and accounting for buffers.
//     (void)stream; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1L;
// }