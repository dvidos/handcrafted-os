#ifndef _TIMER_H
#define _TIMER_H

#include "idt.h"

void init_timer(uint32_t frequency);
void timer_handler(registers_t *regs);
uint64_t timer_get_uptime_ticks();

void pause(int milliseconds);


#endif
