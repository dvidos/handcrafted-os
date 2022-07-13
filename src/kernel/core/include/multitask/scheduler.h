#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <multitask/process.h>
#include <multitask/proclist.h>

// 0=highest priority, 1,2... lower priorities. 
#define PROCESS_PRIORITY_LEVELS   8

// how many msecs to allow each process. Something between 5 and 100
#define DEFAULT_TASK_TIMESLICE_MSECS   30



// the current running process
extern process_t *running_proc;

// ready lists, one per priority 0=high, 3,4,5=lower
extern proc_list_t ready_lists[PROCESS_PRIORITY_LEVELS];

// list of blocked processes, see block_reason and channel
extern proc_list_t blocked_list;

// terminated processes, to be removed by the idle task later
extern proc_list_t terminated_list;

// the next time that timeshare will expire. 
// allows for faster checking in the timer irq handler
extern uint64_t next_switching_time;

// the next time that a sleeping task needs to be awaken
// allows for faster checking in the timer irq handler
extern uint64_t next_wake_up_time;



// use this pair to lock/unlock scheduler
void lock_scheduler();
void unlock_scheduler();

// caller is responsible to always lock/unlock scheduler,
// before calling schedule()
void schedule();



#endif
