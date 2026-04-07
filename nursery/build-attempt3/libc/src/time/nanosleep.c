#include "../libc_internal.h"

/**
 * @brief Suspends the execution of the calling thread for a specified interval.
 *
 * This function suspends the execution of the calling thread until the time
 * specified by `req` has elapsed, or until a signal is delivered.
 *
 * @param req A pointer to a `struct timespec` specifying the duration of the sleep.
 * @param rem If not NULL, any remaining time that was not slept (e.g., due to a signal)
 *            is stored here.
 * @return 0 on success, or -1 on error with `errno` set. If interrupted by a signal,
 *         `errno` is set to `EINTR`.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `nanosleep` on Linux).
 * It requires interaction with the kernel's scheduler to pause the thread for
 * the specified duration.
 */
// int nanosleep(const struct timespec *req, struct timespec *rem) {
//     // TODO: Implement nanosleep for your operating system.
//     // This typically involves a system call.
//     (void)req; // Suppress unused parameter warning
//     (void)rem; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }