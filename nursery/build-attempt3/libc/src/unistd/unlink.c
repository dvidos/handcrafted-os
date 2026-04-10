#include "../libc_internal.h"

/**
 * @brief Deletes a name and possibly the file it refers to.
 *
 * This function deletes a name from the filesystem. If that name was the
 * last link to a file, the file is deleted and the space it was using
 * is made available. If the name referred to a symbolic link, the link
 * is removed, but the file it referred to is not.
 *
 * @param pathname The path to the name to delete.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int unlink(const char *pathname) {
    return syscall(SYS_UNLINK, (int)pathname, 0, 0, 0, 0);
}