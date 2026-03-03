#include <syscall.h>

#ifndef __is_tests  // for tests, we want the exit() of the host system

// exit the process (also called after main() returns)
int exit(int exit_code) {
    return syscall(SYS_EXIT, exit_code, 0, 0, 0, 0);
}


#endif // not __is_tests