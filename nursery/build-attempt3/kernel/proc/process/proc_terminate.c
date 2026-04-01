#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../memory/kheap.h"
#include "../../memory/vmm.h"
#include "../../drivers/timer.h"
#include "../../logger/logger.h"
#include "../../utils/assert.h"
#include "../../klib/cpu_tools.h"


MODULE("PROC_BLCK", LOG_LEVEL_DEBUG);


// -------------------------------------------------------------

static process_t *reparenting_proc = NULL;
process_t *proc_get_reparenting_proc()                { return reparenting_proc; }
void       proc_set_reparenting_proc(process_t *proc) { reparenting_proc = proc; }


static void reparent_children(process_t *source, process_t *dest) {
    process_t *child = source->children_list;
    while (child != NULL) {
        process_t *original_next = child->next_child;
        log_trace("reparenting child process %s[%d] from %s[%d] to %s[%d]", 
            child->name, child->pid, 
            source->name, source->pid, 
            dest->name, dest->pid);

        proc_remove_child(source, child);
        proc_add_child(dest, child);
        child = original_next;
    }

}


// a task can ask to be terminated
void proc_exit(process_t *proc, int exit_code) {
    log_trace("proc_exit(proc=%p [pid=%d], exit_code=%d)", proc, proc == NULL ? -1 : proc->pid, exit_code);

    lock_scheduler();

    // if this process has children (terminated or running), these will be reparented to pid=1, /bin/init
    ASSERT(reparenting_proc != NULL);
    reparent_children(proc, reparenting_proc);
    
    log_trace("Process %s[%d] exited, exit code %d", proc->name, proc->pid, exit_code);
    proc->state = TERMINATED;
    proc->exit_code = exit_code;
    ASSERT(proc == running_proc);
    // running process is on a variable, not on a list, so not needing to remove it from any list
    // it will stay in its parent's children list, for wait()

    // possibly wake up parent process  
    if (proc->parent != NULL && proc->parent->state == BLOCKED && proc->parent->block_reason == WAIT_CHILD_EXIT)
        proc_unblock(proc->parent);

    // whether we unblocked parent or not, somebody else should run
    schedule();
    unlock_scheduler();
}

/*
    Ok, the plan is:
    - we follow the pattern we use for keyboard input.

    when parent asks for wait()
    - if there are no children at all, return error
    - if there are already dead chldren, extract and return, no switching.
    - if all children are alive, block till one dies. 
        (after unblocked, there will magically be a dead child in its children)

    when a process calls exit()
    - if it has any children, reparent them to init[1]
    - mark as dead, maybe release code+data memory, but not process
    - if parent is blocked on waiting for dead child, wake them up
*/


// wait for any child to exit, returns child's PID
int proc_wait(process_t *proc, int *exit_code) {
    log_trace("proc_wait(process=%s[%d])", proc->name, proc->pid);

    lock_scheduler();

    // log_debug("top of wait() loop, dumping processes");
    // dump_process_table();

    // if no children, there is no point.
    if (!proc_has_children(proc)) {
        unlock_scheduler();
        return ERR_NO_CHILDREN;
    }
    
    // if process has children that are already terminated (zombies), 
    // pick one, get the exit code, and now we can completely cleanup the process struct.
    for (process_t *child = proc->children_list; child != NULL; child = child->next_child) {
        if (child->state != TERMINATED)
            continue;
        log_trace("proc_wait(): %s[%d] found terminated child %s[%d] with exit code %d", proc->name, proc->pid, child->name, child->pid, child->exit_code);
        *exit_code = child->exit_code;
        int pid = child->pid;
        proc_remove_child(proc, child);
        proc_destroy(child);
        unlock_scheduler();
        return pid;
    }

    // otherwise, go to sleep, let the exit() wake us up and set the return code.
    log_trace("proc_wait(process=%s[%d]) will block on WAIT_CHILD", proc->name, proc->pid);
    proc->state = BLOCKED;
    proc->block_reason = WAIT_CHILD_EXIT;
    proc->block_channel = NULL;
    proclist_append(&blocked_list, proc);

    schedule();
    unlock_scheduler();
    return 0;
}
