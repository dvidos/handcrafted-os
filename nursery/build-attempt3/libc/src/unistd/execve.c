#include "../libc_internal.h"

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
 */
int execve(const char *path, char *const argv[], char *const envp[]) {
    return syscall(SYS_EXEC, (int)path, (int)argv, (int)envp, 0, 0);
}
