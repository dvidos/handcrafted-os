#include "../../libc_internal.h"

/**
 * @brief Waits for a specific child process to change state.
 *
 * This function suspends execution of the calling process until a child
 * process specified by `pid` changes state. It provides more control than `wait()`.
 *
 * @param pid Specifies the set of child processes to wait for:
 *            - < -1: waits for any child process whose process group ID is equal to the absolute value of `pid`.
 *            - -1: waits for any child process.
 *            - 0: waits for any child process whose process group ID is equal to that of the calling process.
 *            - > 0: waits for the child whose process ID is equal to the value of `pid`.
 * @param stat_loc If not NULL, a pointer to an integer where the termination
 *                 status of the child process will be stored.
 * @param options A bit mask of options (e.g., `WNOHANG`, `WUNTRACED`, `WCONTINUED`).
 * @return On success, returns the process ID of the child process that changed state.
 *         If `WNOHANG` is used and no child has changed state, 0 is returned.
 *         On error, -1 is returned, and `errno` is set.
 */
pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    return syscall(SYS_WAIT_SPEC_CHILD, (int)pid, (int)stat_loc, options, 0, 0);
}