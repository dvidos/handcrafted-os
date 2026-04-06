#include "../libc_internal.h"

/**
 * @brief Sets a timer to deliver an alarm signal.
 *
 * This function arranges for the system to deliver a `SIGALRM` signal to
 * the calling process after `seconds` real-time seconds. If `seconds` is 0,
 * any pending alarm is canceled.
 *
 * @param seconds The number of seconds until `SIGALRM` is delivered.
 * @return The number of seconds remaining until any previously scheduled alarm
 *         was due to be delivered, or 0 if no alarm was pending.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `alarm` on Linux).
 * It interacts with the kernel's timer facilities.
 */
unsigned int alarm(unsigned int seconds) {
    // TODO: Implement alarm for your operating system.
    // This typically involves a system call.
    (void)seconds; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return 0;
}