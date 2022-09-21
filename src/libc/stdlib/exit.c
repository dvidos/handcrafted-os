#include <syscall.h>

#ifdef __is_libc

// exit the process (also called after main() returns)
int exit(int exit_code) {
    return syscall(SYS_EXIT, exit_code, 0, 0, 0, 0);
}



#endif // __is_libc