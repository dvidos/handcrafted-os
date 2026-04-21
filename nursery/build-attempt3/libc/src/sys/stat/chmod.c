#include "../../libc_internal.h"
#include <sys/stat.h> // For mode_t

/**
 * @brief Changes the permissions of a file.
 *
 * This function changes the file mode bits of the file specified by `pathname`
 * to `mode`.
 *
 * @param pathname The path to the file.
 * @param mode The new file mode (permissions).
 * @return 0 on success, or -1 on error with `errno` set.
 */
int chmod(const char *pathname, mode_t mode) {
    int ret = syscall(SYS_CHMOD, (int)pathname, (int)mode, 0, 0, 0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}