#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "string.h"
#include "lock.h"
#include "memory.h"
#include "timer.h"
#include "cpu.h"
#include "clock.h"
#include "process.h"
#include "proclist.h"
#include "kheap.h"

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






process_t *running_proc;
process_t *idle_task;
process_t *initial_task;
proc_list_t ready_list;
proc_list_t blocked_list;
proc_list_t terminated_list;
volatile int switching_postpone_depth = 0;
volatile bool task_switching_pending = false;
volatile bool process_switching_enabled = false;
uint64_t next_wake_up_time;
uint64_t next_switching_time = 0;
#define DEFAULT_TASK_TIMESLICE_MSECS   50



static void schedule();


process_t *running_process() {
    return running_proc;
}


void lock_scheduler() {
    pushcli();
    switching_postpone_depth++;
}

void unlock_scheduler() {
    switching_postpone_depth--;
    if (switching_postpone_depth == 0) {
        // if there was a need to switch, while postponed,
        // do it before we enable interrupts again
        if (task_switching_pending) {
            task_switching_pending = false;
            schedule();
        }
    }
    popcli();
}

// this is how the running task can block itself
void block_me(int reason, void *channel) {
    lock_scheduler();
    running_proc->state = BLOCKED;
    running_proc->block_reason = reason;
    running_proc->block_channel = channel;
    append(&blocked_list, running_proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// this is how a task can get blocked


// this is how someone can unblock a different process
void unblock_process(process_t *proc) {
    if (proc->state != BLOCKED)
        return;
    lock_scheduler();
    unlist(&blocked_list, proc);
    proc->state = READY;
    proc->block_reason = 0;
    prepend(&ready_list, proc);

    // if only the idle task is running, we can preempt it
    if (running_proc == idle_task)
        schedule();
    unlock_scheduler();
}

// a task can ask to sleep for some time
void sleep_me_for(int milliseconds) {
    if (milliseconds <= 0)
        return;
    lock_scheduler();

    running_proc->wake_up_time = timer_get_uptime_msecs() + milliseconds;
    running_proc->state = BLOCKED;
    running_proc->block_reason = SLEEPING;

    // keep the earliest wake up time, useful for fast comparison
    next_wake_up_time = (next_wake_up_time == 0)
        ? running_proc->wake_up_time
        : min(next_wake_up_time, running_proc->wake_up_time);
    
    append(&blocked_list, running_proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// called by the timer handler
static void wake_sleeping_tasks() {
    if (blocked_list.head == NULL)
        return;
    lock_scheduler();

    // move everything to a temp list, then deal with one task at a time
    // tasks are either put back into the sleeping list, or in the ready list.
    proc_list_t temp_list;
    temp_list.head = blocked_list.head;
    temp_list.tail = blocked_list.tail;
    blocked_list.head = NULL;
    blocked_list.tail = NULL;
    uint64_t now = timer_get_uptime_msecs();

    // update the next wake_up_time
    next_wake_up_time = 0;
    process_t *proc = dequeue(&temp_list);
    while (proc != NULL) {
        if (now >= proc->wake_up_time) {
            proc->state = READY;
            prepend(&ready_list, proc);
        } else {
            append(&blocked_list, proc);
            next_wake_up_time = (next_wake_up_time == 0)
                ? proc->wake_up_time
                : min(next_wake_up_time, proc->wake_up_time);
        }
        proc = dequeue(&temp_list);
    }
    
    unlock_scheduler();
}

// a task can ask to be terminated
void terminate_me() {
    lock_scheduler();
    running_proc->state = TERMINATED;
    append(&terminated_list, running_proc);
    schedule();
    unlock_scheduler();
}

static void task_main_wrapper() {
    // unlock the scheduler in our first execution
    unlock_scheduler(); 

    // call the entry point method
    // only the running task would execute this method
    running_proc->entry_point();

    // terminate and later free the process
    terminate_me();
}

// a way to create process
process_t *create_process(func_ptr entry_point, char *name) {
    int stack_size = 4096;
    process_t *p = (process_t *)kalloc(sizeof(process_t));
    char *stack_ptr = kalloc(stack_size);
    p->stack_buffer = stack_ptr;
    p->esp = (uint32_t)(stack_ptr + stack_size - sizeof(switched_stack_snapshot_t) - 64);
    p->stack_snapshot->return_address = (uint32_t)task_main_wrapper;
    p->entry_point = entry_point;
    p->name = name;
    p->cpu_ticks_total = 0;
    p->cpu_ticks_last = 0;
    p->state = READY;

    return p;
}


static void idle_task_main() {
    // this task must not sleep or block
    while (true) {

        // maybe not ideal for an idle task, 
        // but maybe we can use it for some housekeeping
        while (terminated_list.head != NULL) {
            process_t *proc = dequeue(&terminated_list);
            // clean up things here or in a function
            if (proc->stack_buffer != NULL)
                kfree(proc->stack_buffer);
            kfree(proc);
        }
        
        asm("hlt");
    }
}

// caller is responsible for locking interrupts before calling us
static void schedule() {
    // allow locking of switching, to allow multiple tasks to be unlbocked
    if (switching_postpone_depth > 0) {
        task_switching_pending = true;
        return;
    }

    process_t *next = dequeue(&ready_list);
    if (next == NULL)
        return; // nothing to switch to
    
    // if current task is running (as opposed to be blocked or sleeping), put back to the ready list
    process_t *previous = running_proc;
    if (previous->state == RUNNING) {
        previous->state = READY;
        append(&ready_list, previous);
    }

    // before switching, some house keeping
    previous->cpu_ticks_total += (timer_get_uptime_msecs() - previous->cpu_ticks_last);

    // mark the new running proc, "next" variable will have a different value afterwards
    running_proc = next;
    running_proc->state = RUNNING;
    next_switching_time = timer_get_uptime_msecs() + DEFAULT_TASK_TIMESLICE_MSECS;

    /**
     * -------------------------------------------------------------------
     * completely unintiutive, but immensely important:
     * before and after this call, we are in a different stack frame.
     * the values of all arguments and local variables are different!!!!!
     * for example, after the switch, the "old" becomes whatever was used 
     * to switch out the thing we are going to switch in!!!!
     * so, be careful what the expectations are before and after calling this method.
     * -------------------------------------------------------------------
     */
    low_level_context_switch(
        &previous->esp,
        &running_proc->esp
    );

    running_proc->cpu_ticks_last = timer_get_uptime_msecs();
}

void init_multitasking() {
    // we should not neglect the original task that has been running since boot
    // this is what we will switch "from" into whatever other task we want to spawn.
    // this way we always have a "from" to switch from...
    memset((char *)&ready_list, 0, sizeof(ready_list));
    memset((char *)&blocked_list, 0, sizeof(blocked_list));
    memset((char *)&terminated_list, 0, sizeof(terminated_list));

    // at least two tasks: 
    // 1. this one, that has to be marked as RUNNING, to be swapped out
    // 2. an idle one, that will never block or sleep, to be aggressively preempted
    initial_task = create_process(NULL, "Initial");
    idle_task = create_process(idle_task_main, "Idle");

    running_proc = initial_task;
    running_proc->state = RUNNING;
    append(&ready_list, idle_task);
}

void start_process(process_t *process) {
    append(&ready_list, process);
}

// this will never return
void start_multitasking() {
    // flag to our interrupt handler that we can start scheduling
    // after a while, the timer will switch us out and will switch something else in.
    process_switching_enabled = true;


    int delay = 0;
    while (true) {
        clock_time_t t;
        get_real_time_clock(&t);
        printf("This is the root task, time is %02d:%02d:%02d\n", t.hours, t.minutes, t.seconds);
        sleep_me_for(1000);

        if (++delay > 5) {
            printf("\n");
            dump_process_table();
            delay = 0;
        }
    }
}

void multitasking_timer_ticked() {
    if (!process_switching_enabled)
        return;
    
    lock_scheduler();
    uint64_t uptime_msecs = timer_get_uptime_msecs();
    if (next_wake_up_time > 0 && uptime_msecs >= next_wake_up_time) {
        wake_sleeping_tasks();
    }
    if (next_switching_time > 0 && uptime_msecs >= next_switching_time) {
        // i think that to be able to switch during IRQ, our first switching must be 
        // done through IRQ, meaning, all the new task stacks should return to the IRQ handler.
        schedule();
    }
    unlock_scheduler();
}


static void dump_process(process_t *proc) {
    char *states[] = {
        "READY",
        "RUNNING",
        "SLEEPING",
        "BLOCKED",
        "TERMINATED"
    };
    printf("%-10s %08x %08x %-10s %3d %4us\n", 
        proc->name, 
        proc->esp, 
        proc->entry_point,
        (char *)states[(int)proc->state],
        proc->block_reason,
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
    printf("Process list:\n");
    printf("Name       ESP      EIP      State      Blk    CPU\n");
    dump_process(running_proc);
    dump_process_list(&ready_list);
    dump_process_list(&blocked_list);
    dump_process_list(&terminated_list);
}


semaphore_t *create_semaphore(int limit) {
    semaphore_t *semaphore = kalloc(sizeof(semaphore_t));
    memset((char *)semaphore, 0, sizeof(semaphore_t));
    semaphore->limit = limit;
    return semaphore;
}

void acquire_semaphore(semaphore_t *semaphore) {
    lock_scheduler();

    if (semaphore->count < semaphore->limit) {
        // we got it!
        semaphore->count++;
    } else {
        // we cannot acquire, we'll just block until it's free
        semaphore->waiting_processes++;
        block_me(SEMAPHORE, semaphore);
    }

    unlock_scheduler();
}

void release_semaphore(semaphore_t *semaphore) {
    lock_scheduler();

    bool unblocked_a_process = false;
    if (semaphore->waiting_processes > 0) {
        // if there are processes waiting, let's unblock them now
        process_t *proc = blocked_list.head;
        while (proc != NULL) {
            if (proc->state == BLOCKED 
                && proc->block_reason == SEMAPHORE 
                && proc->block_channel == semaphore
            ) {
                unblock_process(proc);
                unblocked_a_process = true;
                break;
            }
            proc = proc->next;
        }
    }
    if (unblocked_a_process) {
        // somebody was waiting and we liberated them
        // we don't lower the count, they now hold the semaphore
        semaphore->waiting_processes--;
    } else {
        // either nobody was waiting, or we did not find them (they may have been killed)
        // lower number as expected
        semaphore->count--;
    }

    unlock_scheduler();
}

mutex_t *create_mutex() {
    return (mutex_t *)create_semaphore(1);
}

void acquire_mutex(mutex_t *mutex) {
    acquire_semaphore((semaphore_t *)mutex);
}

void release_mutex(mutex_t *mutex) {
    release_semaphore((semaphore_t *)mutex);
}
