#include "../libc_internal.h"

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
 */
off_t lseek(int fd, off_t offset, int whence) {
    return syscall(SYS_SEEK, fd, offset, whence, 0, 0);
}