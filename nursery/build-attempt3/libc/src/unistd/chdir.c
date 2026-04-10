#include "../libc_internal.h"

/**
 * @brief Changes the current working directory.
 *
 * This function changes the current working directory of the calling process
 * to the directory specified by `path`.
 *
 * @param path The path to the new current working directory.
 * @return 0 on success, or -1 on error with `errno` set.
 */
int chdir(const char *path) {
    return syscall(SYS_CHDIR, (int)path, 0, 0, 0, 0);
}
