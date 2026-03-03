#include <syscall.h>



// willingly give up CPU for others
int yield() {
    return syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

