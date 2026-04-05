#include "libc_internal.h"

/**
 * @brief Closes a directory stream.
 *
 * This function closes the directory stream `dirp` and frees any resources
 * associated with it.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @return On success, 0 is returned. On error, -1 is returned, and `errno` is set.
 *
 * @implNote
 * A typical implementation would:
 * 1. Close the underlying file descriptor associated with the directory stream
 *    using a system call (e.g., `close`).
 * 2. Free any dynamically allocated memory for the `DIR` structure and its internal buffers.
 * 3. Handle errors from the system call.
 */
int closedir(DIR *dirp) {
    // TODO: Implement closedir for your operating system.
    (void)dirp; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}