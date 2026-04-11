#include "../../libc_internal.h"

/**
 * @brief Waits for any child process to change state.
 *
 * This function suspends execution of the calling process until one of its
 * child processes terminates. If a `stat_loc` argument is non-NULL, the
 * exit status of the terminated child is stored there.
 *
 * @param stat_loc If not NULL, a pointer to an integer where the termination
 *                 status of the child process will be stored.
 * @return On success, returns the process ID of the terminated child. On error,
 *         -1 is returned, and `errno` is set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `wait4` or `waitid`
 * with appropriate flags). The kernel manages the process states and notifies
 * the parent when a child changes state.
 */
pid_t wait(int *stat_loc) {
    while (true) {
        int pid = syscall(SYS_WAIT_ANY_CHILD, (int)stat_loc, 0, 0, 0, 0);
        if (pid == ERR_AGAIN) {
            continue;
        }

        return pid;
    }
}