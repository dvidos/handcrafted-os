#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Flushes all modified file system buffers to disk.
 *
 * This function causes all buffered modifications to file data and metadata
 * to be written to the underlying storage devices.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `sync` on Linux).
 * It's a system-wide operation, often used before system shutdown to ensure
 * data integrity.
 */
void sync(void) {
    // TODO: Implement sync for your operating system.
    // This typically involves a system call.
    errno = ENOSYS; // Function not implemented (no return value, so errno usage is limited)
}