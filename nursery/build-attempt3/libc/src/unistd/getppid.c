#include "../libc_internal.h"

/**
 * @brief Gets the parent process ID of the calling process.
 *
 * This function returns the process ID (PID) of the parent of the calling process.
 *
 * @return The parent process ID of the calling process.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `getppid` on Linux).
 * The kernel tracks the parent-child relationships between processes.
 */
pid_t getppid(void) {
    // TODO: Implement getppid for your operating system.
    // This typically involves a system call.
    errno = ENOSYS; // Function not implemented
    return (pid_t)-1;
}