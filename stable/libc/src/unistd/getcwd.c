#include "../libc_internal.h"

/**
 * @brief Gets the current working directory.
 *
 * This function copies the absolute pathname of the current working directory
 * into the buffer `buf`, which is of size `size`.
 *
 * @param buf The buffer to store the current working directory path.
 * @param size The size of the buffer.
 * @return A pointer to the buffer `buf` on success, or NULL on error with `errno` set.
 */
char *getcwd(char *buf, size_t size) {
    error_t err = syscall(SYS_GET_CWD, (int)buf, (int)size, 0, 0, 0);
    if (err < 0) {
        errno = err;
        return NULL;
    }
    return buf;
}
