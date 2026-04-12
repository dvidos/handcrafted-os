#ifndef _TIMER_H
#define _TIMER_H

#include "../arch/stack_frames.h"


void init_timer();
void timer_interrupt_handler(interrupt_frame_t *regs);
uint64_t timer_get_uptime_msecs();

void timer_pause_blocking(int milliseconds);


#endif
