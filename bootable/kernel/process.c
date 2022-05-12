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








// /////////////////////////////////////////////////////////////////////
 /**
 * this method, written in assembly, performs a task switch
 * it takes two pointers to a uint32_t value
 * - first, it pushes a lot of registers on the stack, 
 * - then saves the ESP into the location pointed by the first argument.
 * - then it takes the value pointed by the second argument and sets ESP
 * - then it pops registers in the reverse order.
 * it will return to the caller whose ESP was saved as the second argument
 * 
 * if both pointers point to the same address, no apparent change will happen
 *
 * After the call, the old_esp value will point to the bottom of the saved stack
 * The stack_snapshot structure maps fields to what should be there in memory
 * if we make a stack_snapshot pointer to point to that value, we can see what's pushed
 * If we prepare such a structure, we can create a new task to switch to.
 * The way things are pushed and the stack_snapshot struct must be kept in sync
 */
extern void low_level_context_switch(uint32_t *old_esp_ptr, uint32_t *new_esp_ptr);

/**
 * this is what's pushed when switching and is used to prepare the target return
 * first entries in the structure are what has been pushed last,
 * or first entries is what will be popped first
 * the structure allows us to prepare new stack snapshot for starting new processes
 * see relevant assembly function
 */
struct switched_stack_snapshot {
    // these are explicitly pushed by us
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eflags;
    // this one is not pushed by our code, but by whoever calls our assembly method
    uint32_t return_address; 
} __attribute__((packed));
typedef struct switched_stack_snapshot switched_stack_snapshot_t;


enum process_state { READY, RUNNING, SLEEPING, BLOCKED, TERMINATED };
struct process {
    char *name;
    struct process *next;
    void *stack_page;
    func_ptr entry_point;
    union { // two views of the same piece of information
        uint32_t esp;
        void *stack_snapshot;
    };
    uint64_t cpu_ticks_total;
    uint64_t cpu_ticks_last;
    enum process_state state;
    int block_reason;
    uint64_t wake_up_time;
};
typedef struct process process_t;

struct proc_list {
    process_t *head;
    process_t *tail;
};
typedef struct proc_list proc_list_t;


process_t kernel_procs[8];
process_t *running_proc;
process_t *idle_task;
proc_list_t ready_list;
proc_list_t blocked_list;
proc_list_t sleeping_list;
proc_list_t terminated_list;
volatile int postpone_switching_tasks = 0;
volatile bool task_switching_pending = false;
volatile bool process_switching_enabled = false;
uint64_t next_wake_up_time;
uint64_t next_switching_time = 0;
#define DEFAULT_TASK_TIMESLICE_MSECS   50

static void schedule();
void dump_process_table();


// add a process at the end of the list. O(1)
void append(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->next = NULL;
    } else {
        list->tail->next = proc;
        list->tail = proc;
        proc->next = NULL;
    }
}

// add a process at the start of the list. O(1)
void prepend(proc_list_t *list, process_t *proc) {
    if (list->head == NULL) {
        list->head = proc;
        list->tail = proc;
        proc->next = NULL;
    } else {
        proc->next = list->head;
        list->head = proc;
    }
}

// extract a process from the start of the list. O(1)
process_t *dequeue(proc_list_t *list) {
    if (list->head == NULL)
        return NULL;
    process_t *proc = list->head;
    list->head = proc->next;
    if (list->head == NULL)
        list->tail = NULL;
    proc->next = NULL;
    return proc;
}

// remove an element from the list. O(n)
void unlist(proc_list_t *list, process_t *proc) {
    if (list->head == proc) {
        dequeue(list);
        return;
    }
    process_t *trailing = list->head;
    while (trailing != NULL && trailing->next != proc)
        trailing = trailing->next;
    
    if (trailing == NULL)
        return; // we could not find entry
    
    trailing->next = proc->next;
    if (list->tail == proc)
        list->tail = trailing;
    proc->next = NULL;
}

void lock_scheduler() {
    pushcli();
    postpone_switching_tasks++;
}

void unlock_scheduler() {
    postpone_switching_tasks--;
    if (postpone_switching_tasks == 0) {
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
void block_me(int reason) {
    lock_scheduler();
    running_proc->state = BLOCKED;
    running_proc->block_reason = reason;
    append(&blocked_list, running_proc);
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
    running_proc->state = SLEEPING;
    running_proc->wake_up_time = timer_get_uptime_msecs() + milliseconds;

    // keep the earliest wake up time, useful for fast comparison
    next_wake_up_time = (next_wake_up_time == 0)
        ? next_wake_up_time = running_proc->wake_up_time
        : min(next_wake_up_time, running_proc->wake_up_time);
    
    append(&sleeping_list, running_proc);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// called by the timer handler
static void wake_sleeping_tasks() {
    if (sleeping_list.head == NULL)
        return;
    lock_scheduler();

    // move everything to a temp list, then deal with one task at a time
    // tasks are either put back into the sleeping list, or in the ready list.
    proc_list_t temp_list;
    temp_list.head = sleeping_list.head;
    temp_list.tail = sleeping_list.tail;
    sleeping_list.head = NULL;
    sleeping_list.tail = NULL;
    uint64_t now = timer_get_uptime_msecs();

    // update the next wake_up_time
    next_wake_up_time = 0;
    process_t *proc = dequeue(&temp_list);
    while (proc != NULL) {
        if (now >= proc->wake_up_time) {
            proc->state = READY;
            prepend(&ready_list, proc);
        } else {
            append(&sleeping_list, proc);
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
    running_proc->entry_point();

    // terminate and later free the process
    terminate_me();
}

// a way to create process
void create_process(void *process, func_ptr entry_point, char *name) {
    process_t *p = (process_t *)process;
    char *stack_ptr = allocate_kernel_page();
    p->stack_page = stack_ptr;
    p->esp = (uint32_t)(stack_ptr + kernel_page_size() - sizeof(switched_stack_snapshot_t) - 64);
    // ((switched_stack_snapshot_t *)p->stack_snapshot)->return_address = (uint32_t)entry_point;
    ((switched_stack_snapshot_t *)p->stack_snapshot)->return_address = (uint32_t)task_main_wrapper;
    p->entry_point = entry_point;
    p->name = name;
    p->cpu_ticks_total = 0;
    p->cpu_ticks_last = 0;
    p->state = READY;
}

void process_a_main() {

    int i = 10;
    while (true) {
        printf("This is A, i=%d\n", i++);
        sleep_me_for(100);
        printf("A, becoming blocked\n");
        block_me(i);
        if (i > 15)
            terminate_me();
    }
}

void process_b_main() {
    for (int i = 0; i < 10; i++) {
        printf("This is B, i is %d\n", i++);
        sleep_me_for(500);
    }
}
void process_c_main() {
    for (int i = 0; i < 10; i++) {
        printf("Task C here, this is the %d time\n", i);
        sleep_me_for(1000);
    }
}
void process_d_main() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            printf("%d + %d = %d\n", i, j, i + j);
            sleep_me_for(100);
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            printf("%d * %d = %d\n", i, j, i * j);
            sleep_me_for(100);
        }
    }
}

static void idle_task_main() {
    // this task must not sleep or block
    while (true) {

        // maybe not ideal for an idle task, 
        // but maybe we can use it for some housekeeping
        while (terminated_list.head != NULL) {
            process_t *proc = dequeue(&terminated_list);
            // clean up things here or in a function
            if (proc->stack_page != NULL)
                free_kernel_page(proc->stack_page);
            // free process entry too if it is not static
        }
        
        asm("hlt");
    }
}

// caller is responsible for locking interrupts before calling us
static void schedule() {
    // allow locking of switching, to allow multiple tasks to be unlbocked
    if (postpone_switching_tasks > 0) {
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
    memset((char *)kernel_procs, 0, sizeof(kernel_procs));
    memset((char *)&ready_list, 0, sizeof(ready_list));
    memset((char *)&blocked_list, 0, sizeof(blocked_list));
    memset((char *)&sleeping_list, 0, sizeof(sleeping_list));
    memset((char *)&terminated_list, 0, sizeof(terminated_list));
    create_process(&kernel_procs[0], 0x00, "Booted");
    create_process(&kernel_procs[1], idle_task_main, "Idle");
    create_process(&kernel_procs[2], process_a_main, "Proc_A");
    create_process(&kernel_procs[3], process_b_main, "Proc_B");
    create_process(&kernel_procs[4], process_c_main, "C");
    create_process(&kernel_procs[5], process_d_main, "C");

    // identify the idle task to allow for preemptive unblocks
    idle_task = &kernel_procs[1];

    // we must mark the correct status of current task, to enable smooth switch to other tasks
    running_proc = &kernel_procs[0];
    running_proc->state = RUNNING;

    append(&ready_list, &kernel_procs[1]);
    append(&ready_list, &kernel_procs[2]);
    append(&ready_list, &kernel_procs[3]);
    append(&ready_list, &kernel_procs[4]);
    append(&ready_list, &kernel_procs[5]);
}

// this will never return
void start_multitasking() {
    process_switching_enabled = true;
    // after a while, the timer will switch us out and will switch something else in.

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

            if (kernel_procs[1].state == BLOCKED) {
                printf("(unblocking A)");
                unblock_process(&kernel_procs[1]);
            }
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


void dump_process_table() {
    char *states[] = {
        "READY",
        "RUNNING",
        "SLEEPING",
        "BLOCKED",
        "TERMINATED"
    };
    printf("Process list:\n");
    printf("i  Name       ESP      EIP      State      Blk    CPU\n");
    for (int i = 0; i < sizeof(kernel_procs) / sizeof(kernel_procs[0]); i++) {
        process_t proc = kernel_procs[i];
        printf("%-2d %-10s %08x %08x %-10s %3d %4us\n", 
            i, 
            proc.name, 
            proc.esp, 
            ((switched_stack_snapshot_t *)proc.stack_snapshot)->return_address,
            (char *)states[(int)proc.state],
            proc.block_reason,
            (proc.cpu_ticks_total / 1000)
        );
    }
}
