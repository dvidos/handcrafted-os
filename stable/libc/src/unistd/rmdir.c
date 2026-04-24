#include "../libc_internal.h"
#include <errno.h> // For errno
#include <unistd.h> // For rmdir prototype

/**
 * @brief Removes an empty directory.
 *
 * This function deletes the empty directory specified by `pathname`.
 *
 * @param pathname The path to the directory to remove.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `rmdir` on Linux).
 * It will fail if the directory is not empty or if permissions are insufficient.
 */
int rmdir(const char *pathname) {
    int ret = syscall(SYS_RMDIR, (int)pathname, 0, 0, 0, 0);
    if (ret < 0) {
        errno = -ret; // Syscalls typically return negative errno on error
        return -1;
    }
    return 0;
}