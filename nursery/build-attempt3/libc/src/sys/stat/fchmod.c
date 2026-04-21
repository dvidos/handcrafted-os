#include "../../libc_internal.h"
#include <sys/stat.h> // For mode_t

/**
 * @brief Changes the permissions of a file associated with a file descriptor.
 *
 * This function is similar to `chmod()`, but it operates on an open file
 * descriptor `fd` instead of a path. It changes the file mode bits to `mode`.
 *
 * @param fd The file descriptor.
 * @param mode The new file mode (permissions).
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `fchmod` on Linux).
 * It allows changing permissions on an already open file, which can be useful
 * when the path to the file is not easily accessible.
 */
int fchmod(int fd, mode_t mode) {
    int ret = syscall(SYS_FCHMOD, fd, (int)mode, 0, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}