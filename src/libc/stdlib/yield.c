#include <syscall.h>

#ifdef __is_libc


// willingly give up CPU for others
int yield() {
    return syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}


#endif // __is_libc