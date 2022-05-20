#include <stddef.h>
#include "process.h"
#include "proclist.h"
#include "../kheap.h"


// add a process at the end of the list. O(1)
void append(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->next = NULL;
    } else {
        list->tail->next = proc;
        list->tail = proc;
        proc->next = NULL;
    }
}

// add a process at the start of the list. O(1)
void prepend(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->next = NULL;
    } else {
        proc->next = list->head;
        list->head = proc;
    }
}

// extract a process from the start of the list. O(1)
process_t *dequeue(proc_list_t *list) {
    if (list->head == NULL)
        return NULL;
    process_t *proc = list->head;
    list->head = proc->next;
    if (list->head == NULL)
        list->tail = NULL;
    proc->next = NULL;
    return proc;
}

// remove an element from the list. O(n)
void unlist(proc_list_t *list, process_t *proc) {
    if (list->head == proc) {
        dequeue(list);
        return;
    }
    process_t *trailing = list->head;
    while (trailing != NULL && trailing->next != proc)
        trailing = trailing->next;
    
    if (trailing == NULL)
        return; // we could not find entry
    
    trailing->next = proc->next;
    if (list->tail == proc)
        list->tail = trailing;
    proc->next = NULL;
}

