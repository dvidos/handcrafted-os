#include "libc_internal.h"

/**
 * @brief Declares an expected access pattern for file data.
 *
 * This function allows an application to advise the kernel about its expected
 * access patterns for a file, allowing the kernel to optimize its caching and
 * I/O behavior. It does not change the behavior of I/O, only acts as a hint.
 *
 * @param fd The file descriptor for the file.
 * @param offset The starting offset for the region.
 * @param len The length of the region (0 means up to EOF).
 * @param advice The advice to give to the kernel (e.g., POSIX_FADV_NORMAL, POSIX_FADV_SEQUENTIAL).
 * @return On success, 0 is returned. On error, a positive error number is returned.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `fadvise64` on Linux).
 * If the underlying OS does not support such fine-grained advice, this function
 * can simply return 0, indicating success (as it's a hint, not a command).
 */
int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
    // TODO: Implement posix_fadvise for your operating system.
    // This typically involves a system call, or can be a no-op if not supported.
    (void)fd;     // Suppress unused parameter warning
    (void)offset; // Suppress unused parameter warning
    (void)len;    // Suppress unused parameter warning
    (void)advice; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented - this function returns error number directly
    return ENOSYS;
}