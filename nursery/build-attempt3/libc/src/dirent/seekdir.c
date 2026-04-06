#include "../libc_internal.h"

/**
 * @brief Sets the position of the next `readdir()` call in a directory stream.
 *
 * This function sets the position of the directory stream `dirp` to the
 * location specified by `loc`. The value of `loc` should be a value returned
 * by a previous call to `telldir()` on the same directory stream.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @param loc The location to set the stream position to, obtained from `telldir()`.
 *
 * @implNote
 * A typical implementation would:
 * 1. Store `loc` within the `DIR` structure to indicate the desired position.
 * 2. The next call to `readdir` would then use this stored position to seek
 *    to the correct location within the underlying directory file descriptor
 *    before reading entries. This might involve an `lseek` system call
 *    on the directory's file descriptor.
 * 3. The exact mechanism depends on how directory entries are read and buffered
 *    internally by `readdir`.
 */
void seekdir(DIR *dirp, long loc) {
    // TODO: Implement seekdir for your operating system.
    (void)dirp; // Suppress unused parameter warning
    (void)loc;  // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented - this function does not return a value
}