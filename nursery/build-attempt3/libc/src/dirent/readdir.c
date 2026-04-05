#include "libc_internal.h"

/**
 * @brief Reads a directory entry.
 *
 * This function reads the next directory entry from the directory stream `dirp`.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @return On success, a pointer to a `struct dirent` object is returned,
 *         representing the next directory entry. On reaching the end of the
 *         directory stream or on error, NULL is returned. `errno` is set on error.
 *
 * @implNote
 * A typical implementation would:
 * 1. Read raw directory data from the underlying file descriptor (obtained by `opendir`)
 *    using a system call (e.g., `getdents64` on Linux).
 * 2. Parse the raw data into a `struct dirent` structure.
 * 3. Update the internal state of the `DIR` stream (e.g., buffer pointer, offset).
 * 4. Handle end-of-directory conditions and errors from the system call.
 * 5. The `struct dirent` returned is usually a static buffer within the `DIR` object
 *    or a thread-local buffer, which means its content may be overwritten by subsequent
 *    calls to `readdir` or `readdir_r`.
 */
struct dirent *readdir(DIR *dirp) {
    // TODO: Implement readdir for your operating system.
    (void)dirp; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return NULL;
}