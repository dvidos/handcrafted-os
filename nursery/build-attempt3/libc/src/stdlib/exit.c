#include "../libc_internal.h"

/**
 * @brief Causes normal program termination.
 *
 * This function causes normal program termination. It performs cleanup in
 * the following order:
 * 1. Call all functions registered with `atexit()`.
 * 2. Flush all open output streams.
 * 3. Close all open streams.
 * 4. Return control to the host environment.
 *
 * @param status The exit status to return to the parent process.
 *
 * @implNote
 * This is a complex function involving multiple cleanup steps. The actual
 * termination typically involves a system call (e.g., `_exit()` or `exit_group()`).
 */
void exit(int status) {
    // TODO: Implement exit for your operating system.
    // This involves calling atexit handlers, flushing/closing streams, and a system call for termination.
    (void)status; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented (no return value, so errno usage is limited)
    // For systems where exit doesn't return, an infinite loop or system halt
    // is appropriate as a placeholder.
    while (1) {
        // Halt the system or enter a debugger
    }
}