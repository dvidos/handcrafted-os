#ifndef _PROCESS_H
#define _PROCESS_H

#include <ctypes.h>
#include <devices/tty.h>

// posix has it, i think
typedef uint16_t pid_t;

// the process describing structure
typedef struct process process_t;

// a function to execute as the task code
typedef void (* func_ptr)();


// idle runs on the lowest priority level
#define PRIORITY_KERNEL           0
#define PRIORITY_DRIVERS          1
#define PRIORITY_USER_PROGRAM     4
#define PRIORITY_IDLE_TASK        7


// create & initialize a process, don't start it yet, optinal association with a tty
// process_t *create_process(func_ptr entry_point, char *name, uint8_t priority, tty_t *tty);
process_t *create_process(func_ptr entry_point, char *name, void *stack_top, uint8_t priority, tty_t *tty);

// this appends the process on the ready queues
void start_process(process_t *process);

// get the running process
process_t *running_process();

// actions that a running task can use
void yield();  // voluntarily give up the CPU to another task
void sleep(int milliseconds);  // sleep self for some milliseconds
void block_me(int reason, void *channel); // blocks task, someone else must unblock it
void exit(uint8_t exit_code);  // terminate self, give exit code
pid_t getpid(); // get pid of current process


// this is how someone can unblock a different process
void unblock_process(process_t *proc);
void unblock_process_that(int block_reason, void *block_channel);

// utility tools
void dump_process_table();



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
enum block_reasons { SLEEPING = 1, SEMAPHORE, WAIT_USER_INPUT };


// the fundamental process information for multi tasking
struct process {
    uint32_t pid;
    char *name;
    struct process *next; // each process can only belong to one list
    // void *stack_buffer;
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
    tty_t *tty; // if the process has an associated tty
};



#endif
