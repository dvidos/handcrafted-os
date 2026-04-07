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
 *
 * @implNote
 * This function typically maps to the `lseek()` system call on the stream's
 * underlying file descriptor. It also needs to handle the stream's internal
 * buffers, potentially discarding buffered input or flushing buffered output.
 */
// int fseek(FILE *stream, off_t offset, int whence) {
//     // TODO: Implement fseek for your operating system.
//     // This involves seeking the underlying file descriptor and managing stream buffers.
//     (void)stream; // Suppress unused parameter warning
//     (void)offset; // Suppress unused parameter warning
//     (void)whence; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }