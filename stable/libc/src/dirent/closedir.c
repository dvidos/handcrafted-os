#include "../libc_internal.h"

/**
 * @brief Closes a directory stream.
 *
 * This function closes the directory stream `dirp` and frees any resources
 * associated with it.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @return On success, 0 is returned. On error, -1 is returned, and `errno` is set.
 *
 * @implNote
 * A typical implementation would:
 * 1. Close the underlying file descriptor associated with the directory stream
 *    using a system call (e.g., `close`).
 * 2. Free any dynamically allocated memory for the `DIR` structure and its internal buffers.
 * 3. Handle errors from the system call.
 */
int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }

    int ret = syscall(SYS_CLOSE_DIR, dirp->fd, 0, 0, 0, 0);
    if (ret < 0) {
        errno = -ret; // Syscalls typically return negative errno on error
        return -1;
    }

    free(dirp);
    return 0;
}