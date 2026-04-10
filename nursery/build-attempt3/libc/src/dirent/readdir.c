#include "../libc_internal.h"

/**
 * @brief Helper function to convert VFS file type to libc dirent d_type.
 */
static unsigned char _vfs_filetype_to_dirent_d_type(uint32_t vfs_d_type) {
    switch (vfs_d_type & S_IFMT) {
        case S_IFDIR:  return DT_DIR;
        case S_IFCHR:  return DT_CHR;
        case S_IFBLK:  return DT_BLK;
        case S_IFREG:  return DT_REG;
        case S_IFIFO:  return DT_FIFO;
        case S_IFLNK:  return DT_LNK;
        case S_IFSOCK: return DT_SOCK;
        default:       return DT_UNKNOWN;
    }
}

/**
 * @brief Reads a directory entry.
 *
 * This function reads the next directory entry from the directory stream `dirp`.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @return On success, a pointer to a `struct dirent` object is returned,
 *         representing the next directory entry. On reaching the end of the
 *         directory stream or on error, NULL is returned. `errno` is set on error.
 */
struct dirent *readdir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return NULL;
    }

    vfs_dirent_t kernel_dirent_buf;
    int ret = syscall(SYS_READ_DIR, dirp->fd, (int)&kernel_dirent_buf, 0, 0, 0);

    if (ret < 0) {
        if (ret == 0) { // End of directory
            return NULL;
        }
        errno = -ret; // Syscalls typically return negative errno on error
        return NULL;
    }

    // Populate the embedded dirent structure in DIR
    dirp->entry.d_ino = kernel_dirent_buf.d_ino;
    strncpy(dirp->entry.d_name, kernel_dirent_buf.d_name, sizeof(dirp->entry.d_name) - 1);
    dirp->entry.d_name[sizeof(dirp->entry.d_name) - 1] = '\0'; // Ensure null-termination
    dirp->entry.d_type = _vfs_filetype_to_dirent_d_type(kernel_dirent_buf.d_type);

    return &dirp->entry;
}