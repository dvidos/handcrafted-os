#include <syscall.h>

#ifdef __is_libc


int puts(char *message) {
    return syscall(SYS_PUTS, (int)message, 0, 0, 0, 0);
}

int putchar(int c) {
    return syscall(SYS_PUTCHAR, c, 0, 0, 0, 0);
}

int clear_screen() {
    return syscall(SYS_CLEAR_SCREEN, 0, 0, 0, 0, 0);
}

int where_xy(int *x, int *y) {
    return syscall(SYS_WHERE_XY, (int)x, (int)y, 0, 0, 0);
}

int goto_xy(int x, int y) {
    return syscall(SYS_GOTO_XY, x, y, 0, 0, 0);
}

int screen_dimensions(int *cols, int *rows) {
    return syscall(SYS_SCREEN_DIMENSIONS, (int)cols, (int)rows, 0, 0, 0);
}

int get_screen_color() {
    return syscall(SYS_GET_SCREEN_COLOR, 0, 0, 0, 0, 0);
}

int set_screen_color(int color) {
    return syscall(SYS_SET_SCREEN_COLOR, color, 0, 0, 0, 0);
}

#endif // __is_libc