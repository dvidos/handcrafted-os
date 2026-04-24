#include "../../libc_internal.h"

/**
 * @brief Delegates call to device driver
 *
 * Behavior depends on device driver
 *
 * @param fd File descriptor of device (e.g. stdout)
 * @param cmd The command to pass
 * @param arg An argument to pass
 * @return 0 on success, or -1 on error with `errno` set.
 */
int ioctl(int fd, unsigned long int request, unsigned long arg) {
    return syscall(SYS_IOCTL, fd, request, arg, 0, 0);
}
