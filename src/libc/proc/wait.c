#include <ctypes.h>
#include <syscall.h>

#ifdef __is_libc

// wait for any child to exit, returns child's PID, error if no children exist
int wait(int *exit_code) {
    return syscall(SYS_WAIT_CHILD, (int)exit_code, 0, 0, 0, 0);
}


#endif
