#include "libc_internal.h"
#include <errno.h> // For errno

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
    // TODO: Implement rmdir for your operating system.
    // This typically involves a system call.
    (void)pathname; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}