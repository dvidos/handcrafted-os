#ifndef _TIME_H
#define _TIME_H

#include <ctypes.h>


// struture visible both to libc and kernel for syscall
typedef struct clocktime {
    uint16_t years;
    uint8_t months;  // 1=Jan
    uint8_t dow;     // 1=Sunday
    uint8_t days;
    uint8_t hours;   // 24h mode
    uint8_t minutes;
    uint8_t seconds;
} clocktime_t;


// methods supported by userland only
#ifdef __is_libc


void uptime(uint64_t *uptime_msecs);
void clocktime(clocktime_t *time);


#endif // __is_libc
#endif // _TIME_H
