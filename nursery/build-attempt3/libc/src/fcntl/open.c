#include "../libc_internal.h"
#include <stdarg.h> // Required for va_list

/**
 * @brief Opens and optionally creates a file.
 *
 * This function opens the file specified by `pathname`. It supports various
 * flags to control access mode (read, write, read/write), creation, truncation,
 * and other behaviors. If `O_CREAT` is specified, `mode` determines the file
 * permissions for the newly created file.
 *
 * @param pathname The path to the file to open.
 * @param flags Bitwise OR of `O_...` flags (e.g., O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_TRUNC).
 * @param ... An optional `mode_t` argument, required if `O_CREAT` is set, specifying file permissions.
 * @return On success, a new file descriptor (a non-negative integer) is returned.
 *         On error, -1 is returned, and `errno` is set appropriately.
 *
 * @implNote
 * This function typically makes a direct system call to the kernel to perform
 * the file opening operation. The `mode` argument (when present) is masked
 * by the process's `umask` to determine the actual file permissions.
 */
int open(const char *pathname, int flags, ...) {

    va_list args;
    va_start(args, flags);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        mode = va_arg(args, mode_t);
    }
    va_end(args);
    
    return syscall(SYS_OPEN, (int)pathname, flags, mode, 0, 0);
}