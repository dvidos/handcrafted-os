#ifndef _MULTITASK_H
#define _MULTITASK_H

#include "../include/ctypes.h"



// the next time that timeshare will expire. 
// allows for faster checking in the timer irq handler
extern uint64_t next_switching_time;

// the next time that a sleeping task needs to be awaken
// allows for faster checking in the timer irq handler
extern uint64_t next_wake_up_time;


// call this before setting up tasks (use create_process & proc_start)
void init_multitasking();          

// reports whether multitasking has started
bool multitasking_enabled();

// start mutlitasking the started processes. this method never returns.
void start_multitasking();


void advice_on_next_wake_up_time(uint64_t proc_wake_up_time);
void reset_switching_time();

// expected to be called from timer IRQ handler, every msec
void multitasking_timer_ticked();


#endif