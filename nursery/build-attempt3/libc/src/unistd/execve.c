#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Executes a file, replacing the current process image, with specified environment.
 *
 * This function is similar to `execv()`, but it also allows specifying the
 * environment for the new program via the `envp` argument.
 *
 * @param path The path to the executable file.
 * @param argv An array of null-terminated strings that are arguments to the new program.
 * @param envp An array of null-terminated strings, in "KEY=VALUE" format, defining
 *             the environment for the new program.
 * @return On success, `execve` does not return. On error, -1 is returned, and `errno` is set.
 *
 * @implNote
 * This is the most flexible of the `exec` family of functions, often directly
 * mapping to a system call (e.g., `execve` on Linux). It allows complete control
 * over the arguments and environment passed to the new program.
 */
int execve(const char *path, char *const argv[], char *const envp[]) {
    // TODO: Implement execve for your operating system.
    // This typically involves a system call.
    (void)path;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    (void)envp;  // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return -1;
}