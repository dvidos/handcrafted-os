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
 */
unsigned int sleep(unsigned int seconds) {
    return syscall(SYS_SLEEP, seconds * 1000, 0, 0, 0, 0);
}
