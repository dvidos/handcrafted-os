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
    if (proc->parent != NULL && proc->parent->state == BLOCKED) {
        bool wake_them = 
            (proc->parent->block_reason == WAIT_ANY_CHILD) ||
            (proc->parent->block_reason == WAIT_SPEC_CHILD && proc->parent->block_channel == (void *)proc->pid);

        if (wake_them) {
            // the parent to return a try-again to libc
            proc_unblock(proc->parent);
            proc_get_interrupt_frame(proc->parent)->eax = ERR_AGAIN;
        }            
    }

    // whether we unblocked parent or not, somebody else should run
    schedule_another_process();
}


// wait for any child to exit, returns child's PID
int proc_wait(process_t *proc, int *exit_code) {
    process_t *child;
    pid_t pid;

    log_trace("proc_wait(process=%s[%d])", proc->name, proc->pid);

    // if no children, there is no point
    if (!proc_has_children(proc))
        return ERR_NO_CHILDREN;
    
    // if process has children that are already terminated (zombies), 
    // pick one, get the exit code, and now we can completely cleanup the process struct.
    child = proc_find_child_in_state(proc, TERMINATED);
    if (child != NULL) {
        log_trace("proc_wait(): %s[%d] found terminated child %s[%d] with exit code %d", proc->name, proc->pid, child->name, child->pid, child->exit_code);
        *exit_code = child->exit_code;
        pid = child->pid;

        proc_remove_child(proc, child);
        proc_destroy(child);
        return pid;
    }

    proc_block(proc, WAIT_ANY_CHILD, NULL);

    child = proc_find_child_in_state(proc, TERMINATED);
    if (child == NULL)
        panic("Process %s[%d] woken up from terminated child, but no terminated child found!", proc->name, proc->pid);

    log_trace("proc_wait(): %s[%d] found terminated child %s[%d] with exit code %d", proc->name, proc->pid, child->name, child->pid, child->exit_code);
    *exit_code = child->exit_code;
    pid = child->pid;

    proc_remove_child(proc, child);
    proc_destroy(child);
    return pid;
}

// wait for any child to exit, returns child's PID
int proc_waitpid(process_t *proc, pid_t child_pid, int *exit_code, int mode) {
    process_t *child;
    pid_t pid;
    
    log_trace("proc_waitpid(process=%s[%d], pid=%d, mode=%d)", proc->name, proc->pid, child_pid, mode);

    // if no children, there is no point
    if (!proc_has_children(proc))
        return ERR_NO_CHILDREN;

    if (child_pid <= 0) // we don't support these options yet
        return ERR_NOT_SUPPORTED;
    
    // we need to ensure this child exists
    child = proc_find_child(proc, child_pid);
    if (child == NULL)
        return ERR_NOT_FOUND;

    if (child->state == TERMINATED) {
        log_trace("proc_waitpid(): %s[%d] found terminated child %s[%d] with exit code %d", proc->name, proc->pid, child->name, child->pid, child->exit_code);
        *exit_code = child->exit_code;

        proc_remove_child(proc, child);
        proc_destroy(child);
        return child_pid;
    }

    proc_block(proc, WAIT_SPEC_CHILD, (void *)child_pid);

    child = proc_find_child(proc, child_pid);
    if (child == NULL)
        panic("Process %s[%d] woken up from terminated child, but the terminated child was not found!", proc->name, proc->pid);

    ASSERT(child->state == TERMINATED);
    log_trace("proc_waitpid(): %s[%d] found terminated child %s[%d] with exit code %d", proc->name, proc->pid, child->name, child->pid, child->exit_code);
    *exit_code = child->exit_code;

    proc_remove_child(proc, child);
    proc_destroy(child);
    return child_pid;
}
