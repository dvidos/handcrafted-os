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
process_t *create_process(func_ptr entry_point, char *name, pid_t ppid, uint8_t priority, void *stack_top, void *page_directory, tty_t *tty);

// this appends the process on the ready queues
void start_process(process_t *process);

// get the running process
process_t *running_process();

// actions that a running task can use
int wait(int *exit_code); // returns error or exited PID
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
enum block_reasons { SLEEPING = 1, SEMAPHORE, WAIT_USER_INPUT, WAIT_CHILD_EXIT };

// flags of the process (bit numbers)
#define PROCESS_STACK_FRAME_INITIALIZED     0


// the fundamental process information for multi tasking
struct process {
    pid_t pid;
    pid_t parent_pid;
    char name[32+1];

    struct process *next; // each process can only belong to one list
    func_ptr entry_point; // where to jump after initializing this process
    uint8_t  priority;

    uint8_t flags;

    union { // two views of the same piece of information
        uint32_t esp;                               // value of the stack pointer
        switched_stack_snapshot_t *stack_snapshot;  // pointer to pushed data on the stack
    };

    // for housekeeping, not good if runtimes < 1 msecs...
    uint64_t cpu_ticks_total;
    uint64_t cpu_ticks_last;

    // should mirror where the process is: running_proc variable, ready_list, block_list, terminated_list.
    enum process_state state;

    // see relevant enums, populated when a process is blocked
    int block_reason;
    void *block_channel;

    // the msetcs uptime in the future, that we are to be woken up
    uint64_t wake_up_time;

    // exit code, to be used for parent process
    uint8_t exit_code;

    // possibly, the process has an associated tty
    tty_t *tty;

    // if we call the wait() function, these two help populate the data
    pid_t   wait_child_pid;
    uint8_t wait_child_exit_code;

    // for user processes, libc will call sbrk()
    void *heap;           // heap will grow upwards
    uint32_t heap_size;   // to allow sbrk() to work

    // if populated, we can check a magic value at the bottom
    void *stack_bottom;   // to allow check for stack overflow
    uint32_t stack_size;  // to allow us to set ESP correctly

    // if null, use the kernel's page directory, otherwise this.
    void *page_directory;

    // if non-null, our user process is supposed to load this executable
    char *executable_to_load;

    // for the start up data
    char **argv;
    char **envp;
};



#endif
