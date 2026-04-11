#include "../libc_internal.h"

/**
 * @brief Sends a signal to a process or a group of processes.
 *
 * This function sends the signal `sig` to the process specified by `pid`.
 * Special `pid` values can be used to send signals to process groups.
 *
 * @param pid The process ID (or process group ID) to send the signal to.
 * @param sig The signal number to send. If 0, no signal is sent, but error
 *            checking is still performed.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps directly to a `kill` system call in the kernel.
 * The kernel is responsible for delivering the signal to the target process(es).
 */
int kill(pid_t pid, int sig) {
    // TODO: Implement kill for your operating system.
    // This typically involves a system call.
    (void)pid; // Suppress unused parameter warning
    (void)sig; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}