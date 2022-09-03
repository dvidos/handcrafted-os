#include <syscall.h>

#ifdef __is_libc

// sleep for some milliseconds
int sleep(unsigned int milliseconds) {
    return syscall(SYS_SLEEP, (int)milliseconds, 0, 0, 0, 0);
    
}



#endif // __is_libc