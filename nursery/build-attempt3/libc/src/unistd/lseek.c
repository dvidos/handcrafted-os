#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Repositions the offset of the open file associated with `fd`.
 *
 * This function repositions the offset of the open file associated with the
 * file descriptor `fd` to `offset` bytes relative to `whence`.
 * - `SEEK_SET`: `offset` is relative to the beginning of the file.
 * - `SEEK_CUR`: `offset` is relative to the current file position.
 * - `SEEK_END`: `offset` is relative to the end of the file.
 *
 * @param fd The file descriptor.
 * @param offset The offset to apply.
 * @param whence The starting point for the offset (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`).
 * @return The resulting offset from the beginning of the file on success,
 *         or `(off_t)-1` on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `lseek` on Linux).
 * It allows random access within a file.
 */
off_t lseek(int fd, off_t offset, int whence) {
    // TODO: Implement lseek for your operating system.
    // This typically involves a system call.
    (void)fd;     // Suppress unused parameter warning
    (void)offset; // Suppress unused parameter warning
    (void)whence; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return (off_t)-1;
}