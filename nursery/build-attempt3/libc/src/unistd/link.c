#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Creates a new name for a file.
 *
 * This function creates a new link (`newpath`) to an existing file (`oldpath`).
 * Both `oldpath` and `newpath` must reside on the same filesystem.
 *
 * @param oldpath The existing path to the file.
 * @param newpath The new path (link) to create.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `link` on Linux).
 * It creates a "hard link," meaning multiple directory entries refer to the same inode.
 */
int link(const char *oldpath, const char *newpath) {
    // TODO: Implement link for your operating system.
    // This typically involves a system call.
    (void)oldpath; // Suppress unused parameter warning
    (void)newpath; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}