#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>



/**
 * this is what's pushed when switching and is used to prepare the target return
 * first entries in the structure are what has been pushed last,
 * or first entries is what will be popped first
 * the structure allows us to prepare new stack snapshot for starting new processes
 * see relevant assembly function
 */
struct switched_stack_snapshot {
    // these registers explicitly pushed by our code
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

// state of a process. corresponding lists exist
enum process_state { READY, RUNNING, BLOCKED, TERMINATED };

// reasons a process can be blocked
enum block_reasons { SLEEPING = 1, SEMAPHORE };

// a function to execute as the task code
typedef void (* func_ptr)();

// the fundamental process information for multi tasking
struct process {
    uint32_t pid;
    char *name;
    struct process *next; // each process can only belong to one list
    void *stack_buffer;
    func_ptr entry_point;
    uint8_t  priority;
    union { // two views of the same piece of information
        uint32_t esp;
        switched_stack_snapshot_t *stack_snapshot;
    };
    uint64_t cpu_ticks_total;
    uint64_t cpu_ticks_last;
    enum process_state state;
    int block_reason;
    void *block_channel;
    uint64_t wake_up_time;
    uint8_t exit_code;
};
typedef struct process process_t;


// we don't know who "owns" the semaphore at each time,
// e.g. when releasing() we will wake up a waiting process at random
// so the only unblocked process owns the semaphore.
// we could have a process pointer, but it may be more than one owners, if limit > 1
struct semaphore {
    int limit;
    int count;
    int waiting_processes; // how many are waiting to release it
};
typedef struct semaphore semaphore_t;
typedef struct semaphore mutex_t;



/** 
 * ----------------------------------------------------------------
 * Public API, for general kernel usage
 * ----------------------------------------------------------------
 */

// entry points from kernel and timer IRQ
void init_multitasking();          // call this before setting up tasks
void start_multitasking();         // this never returns
void multitasking_timer_ticked();  // this expected to be called from IRQ handler

// utility tools
void dump_process_table();

// actions to manipulate tasks
process_t *create_process(func_ptr entry_point, char *name, uint8_t priority);
void start_process(process_t *process);

// actions that a running task can use
void block_me(int reason, void *channel); // blocks task, someone else must unblock it
void sleep(int milliseconds);  // sleep self for some milliseconds
void exit(uint8_t exit_code);  // terminate self, give exit code
void yield();  // voluntarily give up the CPU to another task

// synchonization functions tasks can use
mutex_t *create_mutex();
void acquire_mutex(mutex_t *mutex);
void release_mutex(mutex_t *mutex);

// semaphores have a limit > 1, allowing a number of acquirers
semaphore_t *create_semaphore(int limit);
void acquire_semaphore(semaphore_t *semaphore);
void release_semaphore(semaphore_t *semaphore);



/** 
 * ----------------------------------------------------------------
 * Semi-private API, for scheduling / multitask subsystem only
 * ----------------------------------------------------------------
 */


// get the running process
process_t *running_process();

// this is how someone can unblock a different process
void unblock_process(process_t *proc);

// to be used by anyone before calling schedule()
void lock_scheduler();

// to be used by everyone, after call to schedule()
void unlock_scheduler();




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


#endif