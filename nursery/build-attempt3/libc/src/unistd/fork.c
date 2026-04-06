#include "../libc_internal.h"

/**
 * @brief Creates a new process.
 *
 * This function creates a new process by duplicating the calling process.
 * The new process (child process) is an exact copy of the calling process
 * (parent process), except for its PID and parent PID.
 *
 * @return On success, the PID of the child process is returned in the parent,
 *         and 0 is returned in the child. On error, -1 is returned in the
 *         parent, and `errno` is set.
 *
 * @implNote
 * This is a fundamental process management system call. The kernel creates
 * a new process control block, copies the parent's memory space (or uses
 * copy-on-write), and sets up the new process's context.
 */
pid_t fork(void) {
    // TODO: Implement fork for your operating system.
    // This typically involves a system call.
    errno = ENOSYS; // Function not implemented
    return (pid_t)-1;
}