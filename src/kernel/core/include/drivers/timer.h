#ifndef _TIMER_H
#define _TIMER_H

#include <idt.h>

void init_timer();
void timer_interrupt_handler(registers_t *regs);
uint64_t timer_get_uptime_msecs();

void timer_pause_blocking(int milliseconds);


#endif
