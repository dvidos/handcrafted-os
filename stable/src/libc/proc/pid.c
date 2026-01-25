#include <ctypes.h>
#include <syscall.h>

#ifdef __is_libc

int getpid() {
    return syscall(SYS_GET_PID, 0, 0, 0, 0, 0);
}


int getppid() {
    return syscall(SYS_GET_PPID, 0, 0, 0, 0, 0);
}



#endif
