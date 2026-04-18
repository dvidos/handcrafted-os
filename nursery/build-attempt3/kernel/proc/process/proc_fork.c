#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_FORK", LOG_LEVEL_TRACE);


// clone, return child's PID on parent, zero on child.
int proc_fork(process_t *proc) {
    log_trace("proc_fork(proc=%p [pid=%d])", proc, proc == NULL ? -1 : proc->pid);

    log_debug_fmt(proc_log_formatter, "parent:", proc);
    
    process_t *child;
    error_t err = process_create_for_fork(proc, &child);
    if (err) return err;

    log_debug_fmt(proc_log_formatter, "child:", child);

    // enqueue child to start when appropriate
    proc_start(child);

    // i guess this is the parent
    return child->pid;
}
