#include "../../libc_internal.h"

/**
 * @brief Gets file status.
 *
 * This function obtains information about the file pointed to by `pathname`
 * and stores it in the `stat` structure pointed to by `buf`.
 *
 * @param pathname The path to the file.
 * @param buf A pointer to a `struct stat` to store file information.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `stat` on Linux).
 * It queries the kernel for various attributes of a file, such as size,
 * permissions, ownership, and timestamps.
 */
int stat(const char *pathname, struct stat *buf) {
    if (!pathname || !buf) {
        errno = EFAULT;
        return -1;
    }

    vfs_stat_t kernel_stat_buf;
    int ret = syscall(SYS_STAT, (int)pathname, (int)&kernel_stat_buf, 0, 0, 0);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    // Map vfs_stat_t to struct stat
    buf->st_dev = (dev_t)kernel_stat_buf.st_dev;      // Potential truncation (uint64_t to uint32_t)
    buf->st_ino = (ino_t)kernel_stat_buf.st_ino;      // Potential truncation (uint64_t to uint32_t)
    buf->st_mode = (mode_t)kernel_stat_buf.st_mode;
    buf->st_nlink = (nlink_t)kernel_stat_buf.st_nlink;
    buf->st_uid = (uid_t)kernel_stat_buf.st_uid;
    buf->st_gid = (gid_t)kernel_stat_buf.st_gid;
    buf->st_rdev = 0; // Not available in vfs_stat_t, set to 0
    buf->st_size = (off_t)kernel_stat_buf.st_size;
    buf->st_blksize = (long)kernel_stat_buf.st_blksize;
    buf->st_blocks = (long)kernel_stat_buf.st_blocks; // Potential truncation (uint64_t to int32_t)
    buf->st_atime = (time_t)kernel_stat_buf.st_atime1; // Potential truncation (uint64_t to int32_t)
    buf->st_mtime = (time_t)kernel_stat_buf.st_mtime1; // Potential truncation (uint64_t to int32_t)
    buf->st_ctime = (time_t)kernel_stat_buf.st_ctime1; // Potential truncation (uint64_t to int32_t)

    return 0;
}