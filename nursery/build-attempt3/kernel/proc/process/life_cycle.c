#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../logger/logger.h"
#include "../../memory/kheap.h"
#include "../../memory/virtmem.h"
#include "../../klib/string.h"
#include "../../klib/strvec.h"



MODULE("PROC_LIFE", LOG_LEVEL_WARN);

static pid_t last_pid = 0;
static lock_t pid_lock = 0;





// starts a process, by putting it on the ready list.
void proc_start(process_t *process) {

    // if we have not started multitasking yet... not much
    if (!multitasking_enabled()) {
        proclist_append(&ready_lists[process->priority], process);
        return;
    }

    lock_scheduler();
    proclist_append(&ready_lists[process->priority], process);

    // if running task is lower priority (e.g. idle task), preempt it
    if (running_process() != NULL && process->priority < running_process()->priority)
        schedule();
    
    unlock_scheduler();
}


static void proc_cleanup() {
    // unlock the scheduler in our first execution
    unlock_scheduler(); 

    // we can now call the entry point.
    // for kernel tasks, this is a method in kernel space.
    // for exec(), this is a kernel method to load and run the executable
    // the called method should not return, but call exit() to exit.
    process_t *r = running_process();
    r->entry_point();

    // terminate and later free the process
    log_warn("process(): It seems main returned");
    proc_exit(r, -7);
}


// create but don't start yet
process_t *create_process(char *name, func_ptr entry_point, uint8_t priority, process_t *parent, tty_t *tty) {
    if (priority >= PROCESS_PRIORITY_LEVELS) {
        log_warn("priority %d requested when we only have %d levels", priority, PROCESS_PRIORITY_LEVELS);
        return NULL;
    }

    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    memset(p, 0, sizeof(process_t));
    
    mutex_acquire(&pid_lock);
    p->pid = ++last_pid;
    mutex_release(&pid_lock);

    p->parent = parent;
    p->priority = priority;
    p->tty = tty;
    p->name = kmalloc(strlen(name) + 1);
    strcpy(p->name, name);
    p->state = READY;

    // every process gets this small stack, to be able to switch in
    // since this is inside kernel's mapped memory, no paging faults should occur
    int stack_size = 4096;
    p->allocated_kernel_stack = kmalloc(stack_size);
    memset(p->allocated_kernel_stack, 0, stack_size);
    *(uint32_t *)p->allocated_kernel_stack = STACK_BOTTOM_MAGIC_VALUE;

    // we now have a small stack to set the "return" address for the first switching.
    // how can we setup the initial stack, i.e. the arguments to that entry point?
    p->esp = (uint32_t)(p->allocated_kernel_stack + stack_size - sizeof(switched_stack_snapshot_t));
    p->stack_snapshot->return_address = (uint32_t)proc_cleanup;
    p->page_directory = vmm_get_kernel_page_directory();  // TODO: break this dependency, use function pointer / interface instead.

    // what our proc_cleanup() should call
    p->entry_point = entry_point;

    // set working directory
    proc_chdir(p, "/");

    log_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}

// a task can ask to be terminated
void proc_exit(process_t *proc, int exit_code) {
    lock_scheduler();

    proc->state = TERMINATED;
    proc->exit_code = exit_code;
    proclist_append(&terminated_list, proc);
    log_trace("Process %s[%d] exited, exit code %d", proc->name, proc->pid, exit_code);

    // possibly wake up parent process
    process_t *parent = proc->parent;
    if (parent != NULL && parent->state == BLOCKED && parent->block_reason == WAIT_CHILD_EXIT) {
        log_trace("Will unblock parent process %s[%d]", parent->name, parent->pid);
        parent->terminated_child_pid = proc->pid;
        parent->terminated_child_exit_code = exit_code;
        log_debug("Added pid %d and exit code %d to parent", proc->pid, exit_code);
        proc_unblock(parent);
    }

    // whether we unblocked parent or not, somebody else should run
    schedule();
    unlock_scheduler();
}


// after a process has terminated, clean up resources
void proc_destroy(process_t *proc) {
    // be careful with the exec() process, it may have allocated more resources
    if (proc->name != NULL)
        kfree(proc->name);

    if (proc->allocated_kernel_stack != 0)
        kfree(proc->allocated_kernel_stack);

    if (proc->page_directory != 0 && proc->page_directory != vmm_get_kernel_page_directory())
        vmm_destroy_page_directory(proc->page_directory);

    if (proc->user_proc.executable_path != NULL)
        kfree(proc->user_proc.executable_path);
    if (proc->user_proc.argv != NULL)
        free_strvec(proc->user_proc.argv);
    if (proc->user_proc.envp != NULL)
        free_strvec(proc->user_proc.envp);
    
    if (proc->curr_dir_path != NULL)
        kfree(proc->curr_dir_path);
    
    // can't think of anything else to free
    kfree(proc);
}

