#include "../libc_internal.h"

/**
 * @brief Gets the process ID of the calling process.
 *
 * This function returns the process ID (PID) of the calling process.
 *
 * @return The process ID of the calling process.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `getpid` on Linux).
 * The kernel assigns and tracks PIDs for all processes.
 */
// pid_t getpid(void) {
//     // TODO: Implement getpid for your operating system.
//     // This typically involves a system call.
//     errno = ENOSYS; // Function not implemented
//     return (pid_t)-1;
// }