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
 */
void exit(int status) {
    syscall(SYS_EXIT, status, 0, 0, 0, 0);
}