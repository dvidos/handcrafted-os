#include <ctypes.h>
#include <syscall.h>
#include <errors.h>
#include <stdlib.h>


// wait for any child to exit, returns child's PID, error if no children exist
int wait(int *exit_code) {
    while (true) {
        int pid = syscall(SYS_WAIT_CHILD, (int)exit_code, 0, 0, 0, 0);
        if (pid == ERR_AGAIN) {
            yield();
            continue;
        }

        return pid;
    }
}


