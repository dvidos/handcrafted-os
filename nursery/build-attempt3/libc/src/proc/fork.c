#include <ctypes.h>
#include <syscall.h>


// returns 0 on child, child's PID on parent, <0 on error
int fork() {
    return syscall(SYS_FORK, 0, 0, 0, 0, 0);
}



