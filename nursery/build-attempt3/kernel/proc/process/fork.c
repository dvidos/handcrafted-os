#include "process.h"


// clone, return child's PID on parent, zero on child.
int proc_fork(process_t *proc) {
    process_t *child = create_process(proc->name, proc->entry_point, proc->priority, proc, proc->tty);
    proc_start(child);

    // need to duplicate stack and heap, memory pages, file handles,
    // then even copy the trap state (e.g. the actual EIP value)
    // to resume exactly where we left off.

    return 1;
}

