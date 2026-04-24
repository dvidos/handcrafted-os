#include "../../libc_internal.h"
#include <errno.h> // For errno
#include <sys/stat.h> // For mode_t

/**
 * @brief Creates a new directory.
 *
 * This function creates a new directory at `pathname` with the specified
 * file mode `mode`. The actual permissions of the created directory are
 * affected by the process's `umask`.
 *
 * @param pathname The path to the new directory.
 * @param mode The file mode (permissions) for the new directory.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int mkdir(const char *pathname, mode_t mode) {
    int ret = syscall(SYS_MKDIR, (int)pathname, (int)mode, 0, 0, 0);
    if (ret < 0) {
        errno = -ret; // Syscalls typically return negative errno on error
        return -1;
    }
    return 0;
}