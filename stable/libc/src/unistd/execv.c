#include "../libc_internal.h"

/**
 * @brief Executes a file, replacing the current process image.
 *
 * This function replaces the current process image with a new process image
 * specified by `path`. `argv` is an array of argument strings passed to
 * the new program. The `environ` variable from the calling process is used.
 *
 * @param path The path to the executable file.
 * @param argv An array of null-terminated strings that are arguments to the new program.
 *             The first argument is typically the program name.
 * @return On success, `execv` does not return. On error, -1 is returned, and `errno` is set.
 */
int execv(const char *path, char *const argv[]) {
    extern char **environ;
    return syscall(SYS_EXEC, (int)path, (int)argv, (int)environ, 0, 0);
}
