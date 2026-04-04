#ifndef _PROCLIST_H
#define _PROCLIST_H

#include "../process/process.h"


// a process list and associated methods, allow O(1) for most operations
struct proc_list {
    process_t *head;
    process_t *tail;
};
typedef struct proc_list proc_list_t;


// 0=highest priority, 1,2... lower priorities. 
#define PROCESS_PRIORITY_LEVELS   8

// how many msecs to allow each process. Something between 5 and 100
#define DEFAULT_TASK_TIMESLICE_MSECS   50


// this is global, to avoid local variables in the scheduler
extern volatile process_t *running_proc;

// ready lists, one per priority 0=high, 3,4,5=lower
extern proc_list_t ready_lists[PROCESS_PRIORITY_LEVELS];

// list of blocked processes, see block_reason and channel
extern proc_list_t blocked_list;





// get pointer to the currently running process
process_t *running_process();

void initialize_process_lists();

// add a process at the end of the list. O(1)
void proclist_append(proc_list_t *list, process_t *proc);

// add a process at the start of the list. O(1)
void proclist_prepend(proc_list_t *list, process_t *proc);

// extract a process from the start of the list. O(1)
process_t *proclist_dequeue(proc_list_t *list);

// remove an element from the list. O(n)
void proclist_remove(proc_list_t *list, process_t *proc);




#endif
