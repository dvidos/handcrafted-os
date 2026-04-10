#ifndef _PROCESS_H
#define _PROCESS_H

#include "../include/ctypes.h"
#include "../utils/mutex.h"
#include "../devices/tty.h"
#include "../filesys/vfs_api.h"
#include "../filesys/fs_drivers/fs_driver.h"
#include "../../memory/vmm.h"

/**
 * Virtual memory map of a process (if user process, not kernel task)
 * 
 * 0x40000000  2GB, top of stack (growing down)
 * 0x00xxxxxx  heap (growing up)
 * 0x00xxxxxx  bss segment
 * 0x00xxxxxx  data segment
 * 0x08000000  128 MB, text segment (0x08048000  typical ELF entry point)
 */

 
// used to detect stack overflows
#define STACK_BOTTOM_MAGIC_VALUE    0x12345678




// posix has it, i think
// typedef uint16_t pid_t;

// the process describing structure
typedef struct process process_t;

// a function to execute as the task code
typedef void (* func_ptr)();

typedef enum proc_priority {
    PRIORITY_KERNEL       = 0,
    PRIORITY_KERNEL_TASK  = 1,
    PRIORITY_DRIVERS      = 2,
    PRIORITY_USER_PROGRAM = 4,
    PRIORITY_IDLE_TASK    = 7
} proc_priority_t;


#define MAX_FILE_HANDLES     16

// state of a process. corresponding lists exist
enum process_state { READY, RUNNING, BLOCKED, TERMINATED };

// reasons a process can be blocked
enum block_reasons { NONE, SLEEPING = 1, SEMAPHORE, WAIT_USER_INPUT, WAIT_CHILD_EXIT };

// flags of the process
#define MAX_PROCESS_ELF_SECTIONS 4 // good enough even for dynamic executable

const char *str_process_state(enum process_state state);
const char *str_block_reason(enum block_reasons reason);



// the fundamental process information for multi tasking
struct process {
    struct process *list_next; // each process can only belong to one list
    lock_t process_lock;

    pid_t pid;
    process_t *parent;
    process_t *children_list; // list of children
    process_t *next_child;  // ptr to next sibling in the parent's list
    char *name;
    bool is_user;
    proc_priority_t priority;
    
    struct memory {
        page_dir_t page_dir;       // defines all the virtual memory mappings of the process
        uint32_t tss_esp0_value;   // top of kernel stack, used in scheduler
        
        mem_region_t kernel_stack; // allocated from kernel heap
        mem_region_t user_stack;   // could have a guard page, for stack underflow
        mem_region_t user_heap;    // could have a guard page, for heap overflow
        mem_region_t elf_sections[MAX_PROCESS_ELF_SECTIONS]; // can be .text, .data, .rodata, .bss, etc.

        // this is the ESP inside ring 0, when we are serving an interrupt (e.g. syscall or switching)
        // if should point to the kernel stack.
        // it is saved when switching out, and put on ESP when switching in.
        // whenever this points (because of how we handle interrupts) there should be a trap_frame_t.
        uint32_t saved_esp;

    } memory;

    // should mirror where the process is: running_proc variable, ready_list, block_list, terminated_list.
    enum process_state state;

    // see relevant enums, populated when a process is blocked
    enum block_reasons block_reason;
    void *block_channel;

    // the msetcs uptime in the future, that we are to be woken up
    uint64_t wake_up_time;

    // exit code, to be used for parent process
    uint8_t exit_code;

    // if parent calls the proc_wait() function, these two help populate the data
    pid_t terminated_child_pid;
    int   terminated_child_exit_code;

    inode_t cwd_node;
    char *cwd_path;

    open_file_t *file_handles[MAX_FILE_HANDLES];
};


static inline pid_t proc_get_pid(process_t *proc) { return proc == NULL ? 0 : proc->pid; }
static inline pid_t proc_get_ppid(process_t *proc) { return proc == NULL ? 0 : (proc->parent == NULL ? 0 : proc->parent->pid); }
static inline pid_t proc_is_user_proc(process_t *proc) { return proc == NULL ? false : proc->is_user; }
static inline pid_t proc_is_kernel_proc(process_t *proc) { return proc == NULL ? false : !proc->is_user; }
static inline pid_t proc_count_elf_sections(process_t *proc) { int count = 0; for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) { if (!mem_region_is_empty(&proc->memory.elf_sections[i])) count++; }; return count; }




// get the running process (converts volatile to steady pointer)
process_t *running_process();

void unblock_process_that(enum block_reasons block_reason, void *block_channel);

// actions that a running task can use
void proc_start(process_t *proc);
void proc_yield(process_t *proc);  // voluntarily give up the CPU to another task
bool proc_has_children(process_t *parent);
void proc_add_child(process_t *parent, process_t *child);
void proc_remove_child(process_t *parent, process_t *child);


// proc_create.c
error_t process_v2_create_for_kernel(const char *name, uintptr_t function_to_call, proc_priority_t priority, process_t **proc_ptr);
error_t process_v2_create_for_spawn(process_t *parent, const char *file_path, char **argv, char **envp, proc_priority_t priority, process_t **proc_ptr);
error_t process_v2_replace_for_exec(process_t *proc, const char *file_path, char **argv, char **envp);
error_t process_v2_create_for_fork(process_t *parent, process_t **proc_ptr);


// proc_terminate.c
process_t *proc_get_reparenting_proc();
void       proc_set_reparenting_proc(process_t *proc);
void proc_exit(process_t *proc, int exit_code); 
int  proc_wait(process_t *proc, int *exit_code); // returns error or exited PID
void proc_destroy(process_t *proc);

// cwd.c
int proc_getcwd(process_t *proc, char *buffer, int size);
int proc_chdir(process_t *proc, const char *path);

// fork.c
int proc_fork(process_t *proc); // clone, return child's PID on parent, zero on child

// exec.c
int proc_execve(process_t *proc, const char *path, char *argv[], char *envp[]);

// spawn.c
int proc_spawnve(process_t *parent, char *path, char *argv[], char *envp[]);
int proc_spawn(process_t *parent, char *path);



// blocking.c
void proc_sleep(process_t *proc, int milliseconds);  // sleep self for some milliseconds
void proc_block(process_t *proc, int reason, void *channel); // blocks task, someone else must unblock it
void proc_unblock(process_t *proc);

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

int proc_dup(process_t *proc, int fd);
int proc_dup2(process_t *source_proc, int source_fd, process_t *target_proc, int target_fd);
int proc_pipe(process_t *proc, int fds[]);


// debug.c
void dump_process_table();
const char *proc_get_status_name(enum process_state state);
const char *proc_get_block_reason_name(enum block_reasons reason);
void proc_log_formatter(log_write_stream_t *stream, va_list args);

#endif
