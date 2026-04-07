#include "../libc_internal.h"

/**
 * @brief Spawns a new process.
 */
pid_t spawn(const char *path, char *const argv[], char *const envp[]) {
    return syscall(SYS_SPAWN, (int)path, (int)argv, (int)envp, 0, 0);
}