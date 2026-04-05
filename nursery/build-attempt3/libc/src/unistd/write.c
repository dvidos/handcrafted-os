#include "libc_internal.h"
#include <errno.h> // For errno

/**
 * @brief Writes data to a file descriptor.
 *
 * This function attempts to write up to `count` bytes from the buffer starting
 * at `buf` to the file descriptor `fd`.
 *
 * @param fd The file descriptor to write to.
 * @param buf The buffer containing the data to write.
 * @param count The maximum number of bytes to write.
 * @return On success, the number of bytes written is returned. On error, -1
 *         is returned, and `errno` is set.
 *
 * @implNote
 * This is a fundamental I/O system call. It directly interacts with the kernel
 * to transfer data from user space to a file or device associated with `fd`.
 */
ssize_t write(int fd, const void *buf, size_t count) {
    // TODO: Implement write for your operating system.
    // This typically involves a system call.
    (void)fd;    // Suppress unused parameter warning
    (void)buf;   // Suppress unused parameter warning
    (void)count; // Suppress unused parameter warning
    errno = ENOSYS; // Function not implemented
    return (ssize_t)-1;
}