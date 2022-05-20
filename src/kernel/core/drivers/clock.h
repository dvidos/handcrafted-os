#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdint.h>
#include "../idt.h"


struct clock_time {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;   // 24h mode
    uint8_t days;
    uint8_t dow;     // 1=Sunday
    uint8_t months;  // 1=Jan
    uint16_t years;
};

typedef struct clock_time clock_time_t;

uint32_t get_uptime_in_seconds();
void get_real_time_clock(clock_time_t *p);


void init_real_time_clock(uint8_t interrupt_divisor);
void real_time_clock_interrupt_interrupt_handler(registers_t *regs);


#endif
