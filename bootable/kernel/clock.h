#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdint.h>


struct clock_time {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t days;
    uint8_t dow;
    uint8_t months;
    uint8_t years;
    uint8_t centuries;
};
typedef struct clock_time clock_time_t;

void get_real_time_clock(clock_time_t *p);
void set_real_time_clock(clock_time_t *p);


#endif
