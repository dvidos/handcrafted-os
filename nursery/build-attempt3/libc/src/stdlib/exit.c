#include <syscall.h>

// exit the process (also called after main() returns)
int exit(int exit_code) {
    return syscall(SYS_EXIT, exit_code, 0, 0, 0, 0);
}
