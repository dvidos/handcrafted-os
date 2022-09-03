#ifndef _PROCESS_H
#define _PROCESS_H

#include <ctypes.h>
#include <devices/tty.h>
#include <filesys/vfs.h>

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

#define MAX_FILE_HANDLES         16

// create & initialize a process, don't start it yet, optinal association with a tty
process_t *create_process(char *name, func_ptr entry_point, uint8_t priority, pid_t ppid, tty_t *tty);

// after a process has terminated, clean up resources
void cleanup_process(process_t *proc);

// this appends the process on the ready queues
void start_process(process_t *process);

// get the running process
process_t *running_process();

// actions that a running task can use
int wait(int *exit_code); // returns error or exited PID
void yield();  // voluntarily give up the CPU to another task
void sleep(int milliseconds);  // sleep self for some milliseconds
void block_me(int reason, void *channel); // blocks task, someone else must unblock it
void proc_exit(uint8_t exit_code);  // terminate self, give exit code
pid_t getpid(); // get pid of current process

// maintaining current working directory
int proc_getcwd(process_t *proc, char *buffer, int size);
int proc_setcwd(process_t *proc, char *path);

// for file handles maintained on the process, CWD taken into accout
int proc_open(process_t *proc, char *name);
int proc_read(process_t *proc, int handle, char *buffer, int length);
int proc_write(process_t *proc, int handle, char *buffer, int length);
int proc_seek(process_t *proc, int handle, int offset, enum seek_origin origin);
int proc_close(process_t *proc, int handle);
int proc_opendir(process_t *proc, char *name);
int proc_readdir(process_t *proc, int handle, dir_entry_t *entry);
int proc_closedir(process_t *proc, int handle);

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

// flags of the process
#define PROC_FLAG_IS_USER_PROCESS     0x01



// the fundamental process information for multi tasking
struct process {
    struct process *next; // each process can only belong to one list

    pid_t pid;
    pid_t parent_pid;
    char *name;
    uint8_t flags;
    uint8_t  priority;

    func_ptr entry_point; // where to jump after initializing this process
    
    // used in switching, two views of the same piece of information
    union { 
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

    // possibly, the process has an associated tty
    tty_t *tty;

    // exit code, to be used for parent process
    pid_t   wait_child_pid;
    uint8_t wait_child_exit_code;

    // if parent calls the wait() function, these two help populate the data
    uint8_t exit_code;

    // allocated from kernel heap
    void *allocated_kernel_stack;

    // each user process can have a specific page_directory
    // use get_kernel_page_directory() for kernel
    void *page_directory;

    // data for loading and running user processes
    struct {
        char *executable_path;

        char **argv;
        char **envp;

        // for user processes, libc will call sbrk()
        void *heap;           // heap will grow upwards
        uint32_t heap_size;   // to allow sbrk() to work

        // used to set and detect stack overflow
        void *stack_bottom;

    } user_proc;

    file_t cwd;
    char *cwd_path;
    file_t file_handles[MAX_FILE_HANDLES];
};



#endif
