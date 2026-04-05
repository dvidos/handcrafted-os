#include "libc_internal.h"

/**
 * @brief Examines and changes a signal action.
 *
 * This function allows examining and/or changing the action associated with
 * a specific signal `signum`. It provides more control over signal handling
 * compared to `signal()`.
 *
 * @param signum The signal number.
 * @param act If not NULL, specifies the new action for `signum`.
 * @param oldact If not NULL, the previous action for `signum` is stored here.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This is the primary POSIX interface for robust signal handling.
 * Its implementation typically involves a system call to configure the kernel's
 * signal dispatching for the specified signal. It needs to handle the `sigaction`
 * structure members, including the handler function, signal mask, and flags.
 */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    // TODO: Implement sigaction for your operating system.
    // This typically involves a system call to set up signal handlers.
    (void)signum; // Suppress unused parameter warning
    (void)act;    // Suppress unused parameter warning
    (void)oldact; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}