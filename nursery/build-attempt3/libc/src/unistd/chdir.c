#include "../libc_internal.h"

/**
 * @brief Changes the current working directory.
 *
 * This function changes the current working directory of the calling process
 * to the directory specified by `path`.
 *
 * @param path The path to the new current working directory.
 * @return 0 on success, or -1 on error with `errno` set.
 *
 * @implNote
 * This function typically maps to a system call (e.g., `chdir` on Linux).
 * The kernel is responsible for tracking the current working directory for each process.
 */
// int chdir(const char *path) {
//     // TODO: Implement chdir for your operating system.
//     // This typically involves a system call.
//     (void)path; // Suppress unused parameter warning
//     errno = ENOSYS; // Function not implemented
//     return -1;
// }