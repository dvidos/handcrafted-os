#include "../libc_internal.h"
#include <unistd.h> // For uid_t, gid_t

/**
 * @brief Changes the owner and group of a file.
 *
 * This function changes the owner (user ID) and group (group ID) of the
 * file specified by `pathname`.
 *
 * @param pathname The path to the file.
 * @param owner The new user ID of the owner.
 * @param group The new group ID of the owner.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int chown(const char *pathname, uid_t owner, gid_t group) {
    int ret = syscall(SYS_CHOWN, (int)pathname, (int)owner, (int)group, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}