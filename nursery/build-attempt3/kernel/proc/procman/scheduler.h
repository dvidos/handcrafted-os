#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "../../include/ctypes.h"



extern volatile bool     proc_switch_needed;
extern volatile uint32_t proc_switch_old_esp_ptr;
extern volatile uint32_t proc_switch_new_cr3;
extern volatile uint32_t proc_switch_new_tss_esp0;
extern volatile uint32_t proc_switch_new_esp;
extern volatile uint32_t proc_switch_tss_address;



// use this pair to lock/unlock scheduler
void lock_scheduler();
void unlock_scheduler();

// caller is responsible to lock/unlock, before calling schedule()
void schedule();



#endif
