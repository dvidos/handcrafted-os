#include "libc_internal.h"

/**
 * @brief Replaces the signal mask and suspends the process until a signal is caught.
 *
 * This function temporarily replaces the calling thread's signal mask with the
 * signal set pointed to by `mask` and then suspends the thread until a signal
 * is delivered whose action is either to execute a signal handler or to terminate
 * the process.
 *
 * @param mask A pointer to the signal set that will temporarily replace the current mask.
 * @return -1, and `errno` is always set to `EINTR` (unless an error occurred before suspension).
 *
 * @implNote
 * This is a fundamental system call for waiting for signals. The kernel handles
 * the atomically changing of the mask and suspending the process. When a signal
 * is caught, the original signal mask is restored.
 */
int sigsuspend(const sigset_t *mask) {
    // TODO: Implement sigsuspend for your operating system.
    // This typically involves a system call.
    (void)mask; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}