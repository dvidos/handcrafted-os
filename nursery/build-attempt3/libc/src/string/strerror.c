#include "libc_internal.h"

/**
 * @brief Returns a string describing the error code.
 *
 * This function maps the error number `errnum` to a descriptive error message
 * string. The string is not to be modified by the program.
 *
 * @param errnum The error number (typically from `errno`).
 * @return A pointer to the error message string.
 *
 * @implNote
 * The strings returned by `strerror` are typically stored in read-only memory.
 * This function needs to be thread-safe in multi-threaded environments,
 * which is why `strerror_r` is often preferred.
 */
char *strerror(int errnum) {
    // TODO: Implement strerror for your operating system.
    // This involves mapping an error number to a human-readable string.
    (void)errnum; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return "Unknown error";
}