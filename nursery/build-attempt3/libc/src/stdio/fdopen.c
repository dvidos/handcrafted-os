#include "libc_internal.h"

/**
 * @brief Associates a stream with an existing file descriptor.
 *
 * This function associates a new `FILE` stream with the existing file descriptor `fd`.
 * The `mode` string specifies the type of access requested (e.g., "r", "w", "a").
 *
 * @param fd An existing open file descriptor.
 * @param mode The access mode string for the new stream.
 * @return On success, a pointer to the new `FILE` object is returned. On error, NULL
 *         is returned, and `errno` is set.
 *
 * @implNote
 * This function is useful for turning a low-level file descriptor into a high-level
 * buffered stream. Its implementation involves:
 * 1. Allocating a `FILE` structure.
 * 2. Initializing the `FILE` structure, setting its internal file descriptor to `fd`.
 * 3. Setting up buffering based on `mode`.
 * 4. Ensuring `mode` matches the access permissions of `fd`.
 */
FILE *fdopen(int fd, const char *mode) {
    // TODO: Implement fdopen for your operating system.
    // This involves creating a FILE stream from an existing file descriptor.
    (void)fd;   // Suppress unused parameter warning
    (void)mode; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}