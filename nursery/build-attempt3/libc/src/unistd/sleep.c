#include "../libc_internal.h"

/**
 * @brief Suspends execution for a specified number of seconds.
 *
 * This function causes the calling thread to sleep until either `seconds`
 * have elapsed or a signal is delivered to the thread whose action is
 * to terminate the thread or to invoke a signal handler.
 *
 * @param seconds The number of seconds to sleep.
 * @return The number of unslept seconds (due to being interrupted by a signal),
 *         or 0 if the full duration was slept.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `sleep` or `nanosleep`
 * on Linux). It interacts with the kernel's scheduler to pause the thread.
 */
// unsigned int sleep(unsigned int seconds) {
//     // TODO: Implement sleep for your operating system.
//     // This typically involves a system call.
//     (void)seconds; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return 0;
// }