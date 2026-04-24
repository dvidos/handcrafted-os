#include <syscall.h>
#include <time.h>

#ifdef __is_libc


void uptime(uint64_t *uptime_msecs) {
    syscall(SYS_GET_UPTIME, (int)uptime_msecs, 0, 0, 0, 0);
}


void clocktime(clocktime_t *time) {
    syscall(SYS_GET_CLOCK, (int)time, 0, 0, 0, 0);
}


#endif // __is_libc
