#include "../../libc_internal.h"

/**
 * @brief Gets file status, without following symbolic links.
 *
 * This function obtains information about the file pointed to by `pathname`
 * and stores it in the `stat` structure pointed to by `buf`. If `pathname`
 * is a symbolic link, `lstat` returns information about the link itself,
 * not the file it refers to.
 *
 * @param pathname The path to the file.
 * @param buf A pointer to a `struct stat` to store file information.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `lstat` on Linux).
 * It's crucial for tools that need to inspect symbolic links directly.
 */
int lstat(const char *pathname, struct stat *buf) {
    // TODO: Implement lstat for your operating system.
    // This typically involves a system call.
    (void)pathname; // Suppress unused parameter warning
    (void)buf;      // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}
