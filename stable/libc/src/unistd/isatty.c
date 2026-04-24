#include "../libc_internal.h"

/**
 * @brief Tests whether a file descriptor refers to a terminal.
 *
 * This function tests whether `fd` is an open file descriptor referring to
 * a terminal (TTY) device.
 *
 * @param fd The file descriptor to test.
 * @return 1 if `fd` refers to a terminal, 0 if not, or -1 on error with `errno` set.
 */
int isatty(int fd) {
    // just try to see if we can get terminal settings
    struct termios tios;
    int error = ioctl(fd, TCGETS, (long)&tios);
    return (error >= 0);
}