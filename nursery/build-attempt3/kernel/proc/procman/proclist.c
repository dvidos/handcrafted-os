#include "../process/process.h"
#include "../multitask.h"
#include "proclist.h"
#include "../memory/kheap.h"
#include "../../klib/string.h"



volatile process_t *running_proc = NULL;  // this must be public, to avoid local vars in scheduler()
proc_list_t ready_lists[PROCESS_PRIORITY_LEVELS];
proc_list_t blocked_list;
proc_list_t terminated_list;




void initialize_process_lists() {
    running_proc = NULL;
    memset((char *)&ready_lists, 0, sizeof(ready_lists));
    memset((char *)&blocked_list, 0, sizeof(blocked_list));
    memset((char *)&terminated_list, 0, sizeof(terminated_list));
}



process_t *running_process() {
    // at this point, we convert from volatile to normal pointer.
    // callers using the returned value are no longer expecting a volatile value
    return multitasking_enabled() ? (process_t *)running_proc : NULL;
}



// add a process at the end of the list. O(1)
void proclist_append(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->list_next = NULL;
    } else {
        list->tail->list_next = proc;
        list->tail = proc;
        proc->list_next = NULL;
    }
}

// add a process at the start of the list. O(1)
void proclist_prepend(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->list_next = NULL;
    } else {
        proc->list_next = list->head;
        list->head = proc;
    }
}

// extract a process from the start of the list. O(1)
process_t *proclist_dequeue(proc_list_t *list) {
    if (list->head == NULL)
        return NULL;
    process_t *proc = list->head;
    list->head = proc->list_next;
    if (list->head == NULL)
        list->tail = NULL;
    proc->list_next = NULL;
    return proc;
}

// remove an element from the list. O(n)
void proclist_remove(proc_list_t *list, process_t *proc) {
    if (list->head == proc) {
        proclist_dequeue(list);
        return;
    }
    process_t *trailing = list->head;
    while (trailing != NULL && trailing->list_next != proc)
        trailing = trailing->list_next;
    
    if (trailing == NULL)
        return; // we could not find entry
    
    trailing->list_next = proc->list_next;
    if (list->tail == proc)
        list->tail = trailing;
    proc->list_next = NULL;
}

