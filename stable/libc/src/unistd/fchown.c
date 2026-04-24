#include "../libc_internal.h"
#include <unistd.h> // For uid_t, gid_t

/**
 * @brief Changes the owner and group of a file associated with a file descriptor.
 *
 * This function is similar to `chown()`, but it operates on an open file
 * descriptor `fd` instead of a path. It changes the owner (user ID) and group (group ID)
 * of the file.
 *
 * @param fd The file descriptor.
 * @param owner The new user ID of the owner.
 * @param group The new group ID of the owner.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `fchown` on Linux).
 */
int fchown(int fd, uid_t owner, gid_t group) {
    int ret = syscall(SYS_FCHOWN, fd, (int)owner, (int)group, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}