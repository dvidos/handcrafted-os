#include <ctypes.h>
#include <syscall.h>


// returns 0 on child, child's PID on parent, <0 on error
int exec(char *path, char **argv, char **envp) {
    return syscall(SYS_SPAWN, (int)path, (int)argv, (int)envp, 0, 0);
}



