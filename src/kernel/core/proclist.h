#ifndef _PROCLIST_H
#define _PROCLIST_H

#include "process.h"


// a process list and associated methods, allow O(1) for most operations
struct proc_list {
    process_t *head;
    process_t *tail;
};
typedef struct proc_list proc_list_t;




// add a process at the end of the list. O(1)
void append(proc_list_t *list, process_t *proc);

// add a process at the start of the list. O(1)
void prepend(proc_list_t *list, process_t *proc);

// extract a process from the start of the list. O(1)
process_t *dequeue(proc_list_t *list);

// remove an element from the list. O(n)
void unlist(proc_list_t *list, process_t *proc);




#endif
