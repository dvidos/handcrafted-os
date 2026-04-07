#include "../libc_internal.h"

/**
 * @brief Suspends the process until a signal is caught.
 *
 * This function causes the calling process (or thread) to sleep until
 * a signal is delivered that either terminates the process or causes
 * the invocation of a signal handler.
 *
 * @return -1, and `errno` is set to `EINTR`.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `pause` on Linux).
 * It's a simple way to wait for any signal.
 */
// int pause(void) {
//     // TODO: Implement pause for your operating system.
//     // This typically involves a system call.
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }