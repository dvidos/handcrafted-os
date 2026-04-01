#include "../../utils/assert.h"
#include "../drivers/screen.h"
#include "../klib/string.h"
#include "../utils/mutex.h"
#include "../drivers/timer.h"
#include "../arch/cpu.h"
#include "../drivers/clock.h"
#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../memory/kheap.h"
#include "../memory/vmm.h"
#include "../logger/logger.h"
#include "../include/uapi/errors.h"
#include "../klib/strvec.h"
#include "../klib/cpu_tools.h"


MODULE("PROC", LOG_LEVEL_DEBUG);


 /*
 processes created by kernel, shell, user running programs, etc.
 main mechanism is fork().
 fork() shares the same code, file descriptors, all.
 the data segment is copied, but not shared, different address space is created
 This is separate than exec(), so that the shell child has the opportunity fo redirect stdin/out/err.
 process termination is through either termination of program (compiler to call exit()),
 premature voluntary exiting, being killed or signalled without handler, bugs etc.
 any signal sent to a process, is also sent to all its children and so on.
 seems like kernel will create init, the first process, for it to execute /etc/rc.
 after /etc/rc, it looks at /etc/ttytab, and forks as many gettty processes.
 they wait for user name, execute the login process on it, if successful, they run user's shell.

 processes states can be: running, ready (runnable), blocked.
 processes can block either by calling block(), by reading input from a pipe or terminal
 when no data is available,
 processes toggle between running and ready by the scheduler
 they become unblocked when the event they're waiting on has happened.

 it appears that an interrupt may cause the disk task to run, to handle the relevant disk interrupt
 that's the pre-emptive nature of interrrupt driven systems.
 the sleeping disk task is because it is blocked, waiting for this interrupt.
 after being woken, it handles the event, then goes back to sleep, waiting another interrupt.

 threads is another whole chapter we want to support. threads are not processes,
 they all share the code and memory space with the other threads of the process.
 threads are children of processes, many attributes come from them (e.g. PID),
 but they have their own: instruction and stack pointer, regs, state.
 we should push threads implementation later in the progress, because of complexity.

 I think in the heart of switching is the logic of:
 - push various things
 - change DS
 - pop the various things
 - return (the return address is in the stack???)
 Essentially, it prepares the return jump by changing stacks.
 We may return to the original caller, if we set up their stack.
 See swth.S and how the "init" process is triggered.
 See https://github.com/tiqwab/xv6-x86_64/blob/master/kern/proc.c#L90-L98

 We should differentiate between kernel processes and user processes.
 We could allocate say, 1MB for kernel, have our pages, our stacks, etc.
 This way, whenever an IP maps to physical > 2MB, it is user space, when < 2MB it's kernel.
 
 The main idea is: we want each process to think that
 - they have the CPU to themselves (we switch them without their knowledge)
 - they have the memory to themselves (we give them a continuous virtual memory space)
 - they have their own stack
 */


// starts a process, by putting it on the ready list.
void proc_start(process_t *process) {
    log_trace("proc_start(process=%p [pid=%d])", process, process == NULL ? -1 : process->pid);

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


// this is how someone can unblock a process by reason
void unblock_process_that(enum block_reasons block_reason, void *block_channel) {
    log_trace("unblock_process_that(block_reason=%d (%s), channel=%p)", block_reason, str_block_reason(block_reason), block_channel);

    if (blocked_list.head == NULL)
        return;
    lock_scheduler();

    process_t *proc = blocked_list.head;
    while (proc != NULL) {
        if (proc->block_reason == block_reason && proc->block_channel == block_channel) {
            log_trace("process %s getting unblocked", proc->name);
            proclist_remove(&blocked_list, proc);
            proc->state = READY;
            proc->block_reason = 0;
            proc->block_channel = NULL;
            proclist_prepend(&ready_lists[proc->priority], proc);
            break;
        }
        proc = proc->list_next;
    }

    // if the running process has a lower priority than the new task,
    // let's preempt it, as we are higher priority, 
    // otherwise, wait till timeshare expiration
    if (proc != NULL && running_process()->priority > proc->priority)
        schedule();
    unlock_scheduler();
}

bool proc_has_children(process_t *parent) {
    return parent->children_list != NULL;
}

void proc_add_child(process_t *parent, process_t *child) {
    ASSERT(parent != NULL);
    ASSERT(child != NULL);

    if (parent->children_list == NULL) {
        parent->children_list = child;
    } else {
        process_t *p = parent->children_list;
        while (p->next_child != NULL)
            p = p->next_child;
        p->next_child = child;
    }
    child->next_child = NULL;

    child->parent = parent;
}

void proc_remove_child(process_t *parent, process_t *child) {
    ASSERT(parent != NULL);
    ASSERT(child != NULL);

    if (parent->children_list == child) {
        parent->children_list = child->next_child;
    } else {
        process_t *p = parent->children_list;
        while (p->next_child != NULL && p->next_child != child)
            p = p->next_child;
        if (p->next_child != NULL)
            p->next_child = p->next_child->next_child;
    }
    child->next_child = NULL;

    child->parent = NULL;
}


// voluntarily give up the CPU to another task
void proc_yield(process_t *proc) {
    log_trace("proc_yield(proc=%p [pid=%d])", proc, proc == NULL ? -1 : proc->pid);

    lock_scheduler();
    if (proc == running_proc) {
        schedule();
    }
    unlock_scheduler();
}


#define CASE(x)   case x: return #x
static char unknown_str_buffer[32];

const char *str_process_state(enum process_state state) {
    switch (state) {
        CASE(READY);
        CASE(RUNNING);
        CASE(BLOCKED);
        CASE(TERMINATED);
    }

    sprintfn(unknown_str_buffer, sizeof(unknown_str_buffer), "(unknown process state: %u)", state);
    return unknown_str_buffer;
}

const char *str_block_reason(enum block_reasons reason) {
    switch (reason) {
        CASE(NONE);
        CASE(SLEEPING);
        CASE(SEMAPHORE);
        CASE(WAIT_USER_INPUT);
        CASE(WAIT_CHILD_EXIT);
    }

    sprintfn(unknown_str_buffer, sizeof(unknown_str_buffer), "(unknown block reason: %u)", reason);
    return unknown_str_buffer;
}