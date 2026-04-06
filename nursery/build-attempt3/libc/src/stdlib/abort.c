#include "../libc_internal.h"

/**
 * @brief Causes abnormal program termination.
 *
 * This function causes abnormal program termination. It typically generates
 * a `SIGABRT` signal, which by default causes a core dump and program termination.
 * No cleanup (like `atexit` handlers or flushing streams) is performed.
 *
 * @implNote
 * This function usually maps to a system call or a sequence of system calls
 * that forcefully terminates the current process. It should not return.
 */
void abort(void) {
    // TODO: Implement abort for your operating system.
    // This typically involves sending a SIGABRT signal to self or a system halt.
    errno = ENOSYS; // Function not implemented (no return value, so errno usage is limited)
    // For systems where abort doesn't return, an infinite loop or system halt
    // is appropriate as a placeholder.
    while (1) {
        // Halt the system or enter a debugger
    }
}