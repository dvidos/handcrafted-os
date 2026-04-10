#include "../libc_internal.h"

/**
 * @brief Gets the process ID of the calling process.
 *
 * This function returns the process ID (PID) of the calling process.
 *
 * @return The process ID of the calling process.
 */
pid_t getpid(void) {
    return (pid_t)syscall(SYS_GET_PID, 0, 0, 0, 0, 0);
}