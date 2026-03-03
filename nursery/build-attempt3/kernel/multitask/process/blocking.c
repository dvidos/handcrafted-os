#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../drivers/timer.h"
#include "../../utils/logger.h"


MODULE("PROC_BLCK", LOG_LEVEL_WARN);


// a task can ask to sleep for some time
void proc_sleep(process_t *proc, int milliseconds) {
    if (milliseconds <= 0)
        return;
    lock_scheduler();

    // log_trace("process %s going to sleep for %d msecs", running_process()->name, milliseconds);
    proc->wake_up_time = timer_get_uptime_msecs() + milliseconds;
    proc->state = BLOCKED;
    proc->block_reason = SLEEPING;
    proc->block_channel = NULL;

    // keep the earliest wake up time, useful for fast comparison
    advice_on_next_wake_up_time(proc->wake_up_time);
    
    proclist_append(&blocked_list, proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// this is how the running task can block itself
void proc_block(process_t *proc, int reason, void *channel) {

    lock_scheduler();
    proc->state = BLOCKED;
    proc->block_reason = reason;
    proc->block_channel = channel;
    proclist_append(&blocked_list, proc);
    log_trace("process %s got blocked, reason %d, channel %p", proc->name, reason, channel);
    schedule(); // allow someone else to run
    unlock_scheduler();
}


// this is how someone can unblock a different process
void proc_unblock(process_t *proc) {
    if (proc->state != BLOCKED)
        return;
    lock_scheduler();
    proclist_remove(&blocked_list, proc);
    proc->state = READY;
    proc->block_reason = 0;
    proc->block_channel = NULL;
    proclist_prepend(&ready_lists[proc->priority], proc);
    log_trace("process %s unblocked and added to ready list", proc->name);

    // if the running process has a lower priority than the new task,
    // let's preempt it, as we are higher priority, 
    // otherwise, wait till timeshare expiration
    if (running_process()->priority > proc->priority)
        schedule();
    unlock_scheduler();
}

// wait for any child to exit, returns child's PID
int proc_wait_child(process_t *proc, int *exit_code) {
    lock_scheduler();

    log_trace("proc_wait_child() called, from process %s[%d]", proc->name, proc->pid);

    if (!proc_has_children(proc)) {
        log_error("Process has no children, exiting!");
        unlock_scheduler();
        return ERR_NOT_SUPPORTED;
    }

    log_trace("proc_wait_child(): process %s[%d] will sleep, waiting for children", proc->name, proc->pid);

    // then block us, let the exit() call wake us up.
    proc->state = BLOCKED;
    proc->block_reason = WAIT_CHILD_EXIT;
    proc->block_channel = NULL;
    proclist_append(&blocked_list, proc);

    schedule(); // allow someone else to run
    unlock_scheduler();

    // in theory we'll be here when exit() will unlock us...
    // maybe the "running_proc" var already has our pointer, so we could hook some exit data there...
    proc = running_process();
    log_debug("proc_wait_child(): after scheduler scheduled, running_process points to %s[%d] (should be the original parent)", proc->name, proc->pid);
    *exit_code = proc->terminated_child_exit_code;
    return proc->terminated_child_pid;
}
