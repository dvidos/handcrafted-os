#ifndef _MULTITASK_H
#define _MULTITASK_H


// call this before setting up tasks (use create_process & start_process)
void init_multitasking();          

// start mutlitasking the started processes. this method never returns.
void start_multitasking();

// expected to be called from timer IRQ handler, every msec
void multitasking_timer_ticked();


#endif