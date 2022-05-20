#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <stdbool.h>
#include "process.h"
#include "proclist.h"


// 0=highest priority, 1,2... lower priorities. 
// idle runs on the lowest priority level
#define PROCESS_PRIORITY_LEVELS         5

// how many msecs to allow each process. Something between 5 and 100
#define DEFAULT_TASK_TIMESLICE_MSECS   30


// use this pair to lock/unlock scheduler
void lock_scheduler();
void unlock_scheduler();

// caller is responsible to always lock/unlock scheduler,
// before calling schedule()
void schedule();


extern process_t *running_proc;
extern process_t *idle_task;
extern process_t *initial_task;
extern proc_list_t ready_lists[PROCESS_PRIORITY_LEVELS];
extern proc_list_t blocked_list;
extern proc_list_t terminated_list;
extern uint64_t next_switching_time;
extern uint64_t next_wake_up_time;


#endif
