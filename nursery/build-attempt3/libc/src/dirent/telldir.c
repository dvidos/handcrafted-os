#include "../libc_internal.h"

/**
 * @brief Returns the current location of a directory stream.
 *
 * This function returns the current location associated with the directory
 * stream `dirp`. The value returned can be passed to `seekdir()` to reposition
 * the stream to the same location later.
 *
 * @param dirp A pointer to an open `DIR` directory stream.
 * @return The current location of the directory stream on success, or -1 on error,
 *         with `errno` set.
 *
 * @implNote
 * A typical implementation would:
 * 1. Return the current offset or internal state identifier from the `DIR` structure.
 * 2. This value must be opaque to the caller and only meaningful when passed back
 *    to `seekdir` for the same directory stream.
 * 3. The internal representation might correspond to the byte offset within the
 *    directory file or an entry index.
 */
long telldir(DIR *dirp) {
    // TODO: Implement telldir for your operating system.
    (void)dirp; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}