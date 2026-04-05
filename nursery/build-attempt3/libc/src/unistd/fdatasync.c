#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Synchronizes a file's in-core data with storage device.
 *
 * This function is similar to `fsync()`, but it only guarantees that data
 * portions of the file are written to the storage device. Metadata (like
 * timestamps) might not be updated until a later time.
 *
 * @param fd The file descriptor to synchronize.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `fdatasync` on Linux).
 * It offers a performance optimization over `fsync` when metadata synchronization
 * is not immediately critical.
 */
int fdatasync(int fd) {
    // TODO: Implement fdatasync for your operating system.
    // This typically involves a system call.
    (void)fd; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}