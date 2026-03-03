#ifndef _PROCESS_H
#define _PROCESS_H

#include "../include/ctypes.h"
#include "../utils/mutex.h"
#include "../devices/tty.h"
#include "../filesys/vfs_api.h"

// used to detect stack overflows
#define STACK_BOTTOM_MAGIC_VALUE    0x12345678


// posix has it, i think
// typedef uint16_t pid_t;

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
    struct process *list_next; // each process can only belong to one list
    lock_t process_lock;

    pid_t pid;
    process_t *parent;
    process_t *children_list; // list of children
    process_t *next_sibling;  // ptr to next sibling in the parent's list
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
    enum block_reasons block_reason;
    void *block_channel;

    // the msetcs uptime in the future, that we are to be woken up
    uint64_t wake_up_time;

    // possibly, the process has an associated tty
    tty_t *tty;

    // exit code, to be used for parent process
    uint8_t exit_code;

    // if parent calls the proc_wait_child() function, these two help populate the data
    pid_t terminated_child_pid;
    int   terminated_child_exit_code;

    // allocated from kernel heap. 
    // used for kernel tasks and for the first stage of loading an executable
    void *allocated_kernel_stack;

    // each user process will have a specific page_directory
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

        // this stack allocated from physical memory
        // along with the other segments (e.g. .code, .data, .bss)
        // stack bottom used to set and detect stack underflow
        void *stack_bottom;
        uint32_t stack_size;

    } user_proc;

    inode_t *curr_dir;
    char *curr_dir_path;
    open_file_t file_handles[MAX_FILE_HANDLES];
};


// get the running process (converts volatile to steady pointer)
process_t *running_process();

void unblock_process_that(enum block_reasons block_reason, void *block_channel);

// actions that a running task can use
void proc_yield(process_t *proc);  // voluntarily give up the CPU to another task
pid_t proc_getpid(process_t *proc); // get pid of current process
pid_t proc_getppid(process_t *proc); // get parent pid of running process
bool proc_has_children(process_t *parent);




// life_cycle.c
process_t *create_process(char *name, func_ptr entry_point, uint8_t priority, process_t *parent, tty_t *tty);
void proc_start(process_t *proc);
void proc_exit(process_t *proc, int exit_code); 
void proc_destroy(process_t *proc);

// cwd.c
int proc_getcwd(process_t *proc, char *buffer, int size);
int proc_chdir(process_t *proc, const char *path);

// fork.c
int proc_fork(process_t *proc); // clone, return child's PID on parent, zero on child

// exec.c
int proc_execve(process_t *proc, const char *path, char *const argv[], char *const envp[]);

// blocking.c
void proc_sleep(process_t *proc, int milliseconds);  // sleep self for some milliseconds
void proc_block(process_t *proc, int reason, void *channel); // blocks task, someone else must unblock it
void proc_unblock(process_t *proc);
int  proc_wait_child(process_t *proc, int *exit_code); // returns error or exited PID

// file_ops.c
int proc_open(process_t *proc, char *name);
int proc_read(process_t *proc, int handle, char *buffer, int length);
int proc_write(process_t *proc, int handle, char *buffer, int length);
int proc_seek(process_t *proc, int handle, int offset, int origin);
int proc_close(process_t *proc, int handle);
int proc_opendir(process_t *proc, char *name);
int proc_rewinddir(process_t *proc, int handle);
int proc_readdir(process_t *proc, int handle, vfs_dirent_t *entry);
int proc_closedir(process_t *proc, int handle);

// debug.c
void dump_process_table();
const char *proc_get_status_name(enum process_state state);
const char *proc_get_block_reason_name(enum block_reasons reason);


#endif
