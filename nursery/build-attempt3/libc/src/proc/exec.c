#include <ctypes.h>
#include <syscall.h>


// returns child's PID or negative error
int exec(char *path, char **argv, char **envp) {
    return syscall(SYS_EXEC, (int)path, (int)argv, (int)envp, 0, 0);
}


