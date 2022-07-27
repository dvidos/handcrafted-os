#include <drivers/screen.h>
#include <klib/string.h>
#include <lock.h>
// #include "../memory/memory.h"
#include <drivers/timer.h>
#include <cpu.h>
#include <drivers/clock.h>
#include <multitask/process.h>
#include <multitask/proclist.h>
#include <memory/kheap.h>
#include <memory/virtmem.h>
#include <klog.h>
#include <multitask/scheduler.h>
#include <errors.h>

#define min(a, b)   ((a) < (b) ? (a) : (b))


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


pid_t last_pid = 0;
lock_t pid_lock = 0;


char *process_state_names[] = { "READY", "RUNNING", "BLOCKED", "TERMINATED" };
char *process_block_reason_names[] = { "", "SLEEPING", "SEMAPHORE", "WAIT USER INPUT", "WAIT CHILD EXIT" };


// starts a process, by putting it on the waiting list.
void start_process(process_t *process) {
    lock_scheduler();
    append(&ready_lists[process->priority], process);

    // if running task is lower priority (e.g. idle task), preempt it
    if (running_proc != NULL && process->priority < running_proc->priority)
        schedule();
    unlock_scheduler();
}


process_t *running_process() {
    return running_proc;
}


// this is how the running task can block itself
void block_me(int reason, void *channel) {
    lock_scheduler();
    running_proc->state = BLOCKED;
    running_proc->block_reason = reason;
    running_proc->block_channel = channel;
    append(&blocked_list, running_proc);
    klog_trace("process %s got blocked, reason %d, channel %p", running_proc->name, reason, channel);
    schedule(); // allow someone else to run
    unlock_scheduler();
}


// this is how someone can unblock a different process
void unblock_process(process_t *proc) {
    if (proc->state != BLOCKED)
        return;
    lock_scheduler();
    unlist(&blocked_list, proc);
    proc->state = READY;
    proc->block_reason = 0;
    proc->block_channel = NULL;
    prepend(&ready_lists[proc->priority], proc);
    klog_trace("process %s unblocked and added to ready list", proc->name);

    // if the running process has a lower priority than the new task,
    // let's preempt it, as we are higher priority, 
    // otherwise, wait till timeshare expiration
    if (running_proc->priority > proc->priority)
        schedule();
    unlock_scheduler();
}

// this is how someone can unblock a process by reason
void unblock_process_that(int block_reason, void *block_channel) {
    if (blocked_list.head == NULL)
        return;
    lock_scheduler();

    process_t *proc = blocked_list.head;
    while (proc != NULL) {
        if (proc->block_reason == block_reason && proc->block_channel == block_channel) {
            klog_trace("process %s getting unblocked", proc->name);
            unlist(&blocked_list, proc);
            proc->state = READY;
            proc->block_reason = 0;
            proc->block_channel = NULL;
            prepend(&ready_lists[proc->priority], proc);
            break;
        }
        proc = proc->next;
    }

    // if the running process has a lower priority than the new task,
    // let's preempt it, as we are higher priority, 
    // otherwise, wait till timeshare expiration
    if (proc != NULL && running_proc->priority > proc->priority)
        schedule();
    unlock_scheduler();
}


// wait for any child to exit
int wait(int *exit_code) {

    // first find if we do have any children!
    bool has_children = false;
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        process_t *p = ready_lists[priority].head;
        while (p != NULL) {
            if (p->parent_pid == running_proc->pid) {
                has_children = true;
                break;
            }
            p = p->next;
        }
        if (has_children)
            break;
    }
    if (!has_children) {
        process_t *p = blocked_list.head;
        while (p != NULL) {
            if (p->parent_pid == running_proc->pid) {
                has_children = true;
                break;
            }
            p = p->next;
        }
    }
    if (!has_children)
        return ERR_NOT_SUPPORTED;

    klog_trace("process %s waiting for children", running_proc->name);
    lock_scheduler();

    // then block us, let the exit() call wake us up.
    running_proc->state = BLOCKED;
    running_proc->block_reason = WAIT_CHILD_EXIT;
    running_proc->block_channel = NULL;

    append(&blocked_list, running_proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
    // in theory we'll be here when exit() will unlock us...
    // maybe the "running_proc" var already has our pointer, so we could hook some exit data there...
    klog_debug("upon unblocking from exit(), running_proc points to \"%s\"", running_proc->name);
    *exit_code = running_proc->wait_child_exit_code;
    return running_proc->wait_child_pid;
}

// voluntarily give up the CPU to another task
void yield() {
    klog_trace("process %s is yielding", running_proc->name);
    lock_scheduler();
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// a task can ask to sleep for some time
void sleep(int milliseconds) {
    if (milliseconds <= 0)
        return;
    lock_scheduler();

    // klog_trace("process %s going to sleep for %d msecs", running_proc->name, milliseconds);
    running_proc->wake_up_time = timer_get_uptime_msecs() + milliseconds;
    running_proc->state = BLOCKED;
    running_proc->block_reason = SLEEPING;
    running_proc->block_channel = NULL;

    // keep the earliest wake up time, useful for fast comparison
    next_wake_up_time = (next_wake_up_time == 0)
        ? running_proc->wake_up_time
        : min(next_wake_up_time, running_proc->wake_up_time);
    
    append(&blocked_list, running_proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// a task can ask to be terminated
void exit(uint8_t exit_code) {
    lock_scheduler();
    running_proc->state = TERMINATED;
    running_proc->exit_code = exit_code;
    append(&terminated_list, running_proc);
    klog_trace("process %s exited, exit code %d", running_proc->name, exit_code);

    // possibly wake up parent process
    process_t *p = blocked_list.head;
    while (p != NULL) {
        if (p->block_reason == WAIT_CHILD_EXIT && p->pid == running_proc->parent_pid) {
            klog_trace("parent process %s getting unblocked", p->name);
            unlist(&blocked_list, p);
            p->state = READY;
            p->block_reason = 0;
            p->block_channel = NULL;
            p->wait_child_pid = running_proc->pid;
            p->wait_child_exit_code = exit_code;
            prepend(&ready_lists[p->priority], p);
            break;
        }
        p = p->next;
    }

    schedule();
    unlock_scheduler();
}

pid_t getpid() {
    return running_proc->pid;
}

pid_t getppid() {
    return running_proc->parent_pid;
}

static void scheduler_unlocking_entry_point() {
    // unlock the scheduler in our first execution
    unlock_scheduler(); 

    // we can now call the entry point.
    // for kernel tasks, this is a method in kernel space.
    // for exec(), this is a kernel method to load and run the executable
    // the called method should not return, but call exit() to exit.

    running_proc->entry_point();

    // terminate and later free the process
    klog_warn("process(): It seems main returned");
    exit(255);
}

process_t *create_process(char *name, func_ptr entry_point, uint8_t priority, pid_t ppid, tty_t *tty) {
    if (priority >= PROCESS_PRIORITY_LEVELS) {
        klog_warn("priority %d requested when we only have %d levels", priority, PROCESS_PRIORITY_LEVELS);
        return NULL;
    }

    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    memset(p, 0, sizeof(process_t));
    
    acquire(&pid_lock);
    p->pid = ++last_pid;
    release(&pid_lock);

    p->parent_pid = ppid;
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
    p->esp = (uint32_t)(p->allocated_kernel_stack + stack_size - sizeof(switched_stack_snapshot_t));
    p->stack_snapshot->return_address = (uint32_t)scheduler_unlocking_entry_point;
    p->page_directory = get_kernel_page_directory();

    // what our scheduler_unlocking_entry_point() should call
    p->entry_point = entry_point;

    klog_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}



// process_t *create_process(func_ptr entry_point, char *name, pid_t ppid, uint8_t priority, void *stack_top, void *page_directory, tty_t *tty) {
//     p->esp = (uint32_t)(stack_top - sizeof(switched_stack_snapshot_t));

//     // when starting a new executable, we have allocated new virtual memory, 
//     // which includes the stack, where the line below tries to write to.
//     // the related page-directory is not switched in yet, we only see
//     // kernel's namespace.
//     // we must find a way to write the return address in that unmapped-yet memory page.

//     // we can temporarily find the actual physical address and map the address 
//     // temporarily somewhere in our address space.
//     // this func does not know if the process' stack is currently map or not.
//     p->stack_snapshot->return_address = (uint32_t)scheduler_unlocking_entry_point;

//     p->entry_point = entry_point; // where scheduler_unlocking_entry_point() will jump to
//     p->page_directory = page_directory;

//     klog_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
//     return p;
// }

// after a process has terminated, clean up resources
void cleanup_process(process_t *proc) {
    // be careful with the exec() process, it may have allocated more resources
    if (proc->name != NULL)
        kfree(proc->name);

    if (proc->allocated_kernel_stack != NULL)
        kfree(proc->allocated_kernel_stack);

    if (proc->page_directory != NULL && proc->page_directory != get_kernel_page_directory())
        destroy_page_directory(proc->page_directory);

    if (proc->user_proc.executable_path != NULL)
        kfree(proc->user_proc.executable_path);
    
    // can't think of anything else to free
    kfree(proc);
}

static void dump_process(process_t *proc) {
    klog_info("%-3d %-10s %08x %08x %-10s %-10s %4us", 
        proc->pid,
        proc->name, 
        proc->esp, 
        proc->entry_point,
        (char *)process_state_names[(int)proc->state],
        (char *)process_block_reason_names[proc->block_reason],
        (proc->cpu_ticks_total / 1000)
    );
}

static void dump_process_list(proc_list_t *list) {
    process_t *proc = list->head;
    while (proc != NULL) {
        dump_process(proc);
        proc = proc->next;
    }
}

void dump_process_table() {
    klog_info("Process list:");
    klog_info("PID Name       ESP      EIP      State      Blck Reasn    CPU");
    dump_process((process_t *)running_proc);
    for (int pri = 0; pri < PROCESS_PRIORITY_LEVELS; pri++) {
        dump_process_list(&ready_lists[pri]);
    }
    dump_process_list(&blocked_list);
    dump_process_list(&terminated_list);
}
