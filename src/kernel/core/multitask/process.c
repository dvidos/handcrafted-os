#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../drivers/screen.h"
#include "../string.h"
#include "../lock.h"
// #include "../memory/memory.h"
#include "../drivers/timer.h"
#include "../cpu.h"
#include "../drivers/clock.h"
#include "process.h"
#include "proclist.h"
#include "../memory/kheap.h"
#include "../klog.h"
#include "scheduler.h"

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


char *process_state_names[] = { "READY", "RUNNING", "BLOCKED", "TERMINATED" };
char *process_block_reason_names[] = { "", "SLEEPING", "SEMAPHORE", "WAIT USER INPUT" };


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
    if (running_proc->priority > proc->priority)
        schedule();
    unlock_scheduler();
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

    klog_trace("process %s going to sleep for %d msecs", running_proc->name, milliseconds);
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
    schedule();
    unlock_scheduler();
}

pid_t getpid() {
    return running_proc->pid;
}

static void task_entry_point_wrapper() {
    // unlock the scheduler in our first execution
    unlock_scheduler(); 

    // call the entry point method
    // only the running task would execute this method
    running_proc->entry_point();

    // terminate and later free the process
    exit(0);
}

// a way to create process
process_t *create_process(func_ptr entry_point, char *name, uint8_t priority) {
    if (priority >= PROCESS_PRIORITY_LEVELS) {
        klog_warn("priority %d requested when we only have %d levels", priority, PROCESS_PRIORITY_LEVELS);
        return NULL;
    }
    
    int stack_size = 4096;
    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    memset(p, 0, sizeof(process_t));
    char *stack_ptr = kmalloc(stack_size);
    memset(stack_ptr, 0, stack_size);
    
    pushcli();
    p->pid = ++last_pid;
    popcli();

    p->stack_buffer = stack_ptr;
    p->esp = (uint32_t)(stack_ptr + stack_size - sizeof(switched_stack_snapshot_t) - 64);
    p->stack_snapshot->return_address = (uint32_t)task_entry_point_wrapper;
    p->entry_point = entry_point;
    p->priority = priority;
    p->name = name;
    p->cpu_ticks_total = 0;
    p->cpu_ticks_last = 0;
    p->state = READY;

    klog_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
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
