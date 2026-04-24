#include "../../libc_internal.h"
#include <errno.h>

/**
 * @brief Gets file status for an open file descriptor.
 *
 * This function obtains information about the file associated with the open
 * file descriptor `fd` and stores it in the `stat` structure pointed to by `buf`.
 *
 * @param fd The file descriptor.
 * @param buf A pointer to a `struct stat` to store file information.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `fstat` on Linux).
 * It queries the kernel for file attributes using an already open file handle.
 */
int fstat(int fd, struct stat *buf) {
    if (fd < 0 || !buf) {
        errno = EFAULT;
        return -1;
    }

    vfs_stat_t kernel_stat_buf;
    int ret = syscall(SYS_FSTAT, (int)fd, (int)&kernel_stat_buf, 0, 0, 0);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    // Map vfs_stat_t to struct stat
    buf->st_dev = (dev_t)kernel_stat_buf.st_dev;
    buf->st_ino = (ino_t)kernel_stat_buf.st_ino;
    buf->st_mode = (mode_t)kernel_stat_buf.st_mode;
    buf->st_nlink = (nlink_t)kernel_stat_buf.st_nlink;
    buf->st_uid = (uid_t)kernel_stat_buf.st_uid;
    buf->st_gid = (gid_t)kernel_stat_buf.st_gid;
    buf->st_rdev = 0; // Not available in vfs_stat_t, set to 0
    buf->st_size = (off_t)kernel_stat_buf.st_size;
    buf->st_blksize = (long)kernel_stat_buf.st_blksize;
    buf->st_blocks = (long)kernel_stat_buf.st_blocks;
    buf->st_atime = (time_t)kernel_stat_buf.st_atime1;
    buf->st_mtime = (time_t)kernel_stat_buf.st_mtime1;
    buf->st_ctime = (time_t)kernel_stat_buf.st_ctime1;

    return 0;
}