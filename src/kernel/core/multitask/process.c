#include <drivers/screen.h>
#include <klib/string.h>
#include <lock.h>
#include <drivers/timer.h>
#include <cpu.h>
#include <drivers/clock.h>
#include <multitask/process.h>
#include <multitask/proclist.h>
#include <multitask/scheduler.h>
#include <multitask/multitask.h>
#include <memory/kheap.h>
#include <memory/virtmem.h>
#include <klog.h>
#include <errors.h>
#include <multitask/process.h>

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


// starts a process, by putting it on the ready list.
void start_process(process_t *process) {

    // if we have not started multitasking yet... not much
    if (!multitasking_enabled()) {
        append(&ready_lists[process->priority], process);
        return;
    }

    lock_scheduler();
    append(&ready_lists[process->priority], process);

    // if running task is lower priority (e.g. idle task), preempt it
    if (running_proc != NULL && process->priority < running_proc->priority)
        schedule();
    
    unlock_scheduler();
}


process_t *running_process() {
    // at this point, we convert from volatile to normal pointer.
    // callers using the returned value are no longer expecting a volatile value
    return (process_t *)running_proc;
}


// this is how the running task can block itself
void proc_block(int reason, void *channel) {
    process_t *p = running_process();

    lock_scheduler();
    p->state = BLOCKED;
    p->block_reason = reason;
    p->block_channel = channel;
    append(&blocked_list, p);
    klog_trace("process %s got blocked, reason %d, channel %p", p->name, reason, channel);
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


// clone, return child's PID on parent, zero on child.
int proc_fork() {
    // similar to create process, 
    // we could create_process(), copy things over,
    // but this is not enough, 
    // we need to bring info from the exec() loader.
    // e.g. copy segments (stack, data, heap) etc. 
    return ERR_NOT_IMPLEMENTED;
}

bool proc_has_children(process_t *parent) {
    klog_debug("Checking if proc %s[%d] has children", parent->name, parent->pid);
    bool has_children = false;
    lock_scheduler();

    // first check the running process
    if (running_proc->parent == parent) {
        klog_debug("The running process (%d) is a parent of %d", running_proc->pid, parent->pid);
        has_children = true;
        goto exit;
    }

    // look at the ready queues
    for (int priority = 0; priority < PROCESS_PRIORITY_LEVELS; priority++) {
        process_t *p = ready_lists[priority].head;
        while (p != NULL) {
            klog_debug("Checking priority %d, proc %s[%d]", priority, p->name, p->pid);
            if (p->parent == parent) {
                klog_debug("Found pid %d as a child of pid %d", p->pid, parent->pid);
                has_children = true;
                goto exit;
            }
            p = p->next;
        }
    }

    // look at block queue
    process_t *p = blocked_list.head;
    while (p != NULL) {
        klog_debug("Checking proc %s[%d]", p->name, p->pid);
        if (p->parent == parent) {
            klog_debug("Found pid %d as a child of pid %d", p->pid, parent->pid);
            has_children = true;
            goto exit;
        }
        p = p->next;
    }

exit:
    unlock_scheduler();
    return has_children;
}

// wait for any child to exit, returns child's PID
int proc_wait_child(int *exit_code) {
    lock_scheduler();

    process_t *p = running_process();
    klog_trace("proc_wait_child() called, from process %s[%d]", p->name, p->pid);

    if (!proc_has_children(p)) {
        klog_error("Process has no children, exiting!");
        unlock_scheduler();
        return ERR_NOT_SUPPORTED;
    }

    klog_trace("proc_wait_child(): process %s[%d] will sleep, waiting for children", p->name, p->pid);

    // then block us, let the exit() call wake us up.
    p->state = BLOCKED;
    p->block_reason = WAIT_CHILD_EXIT;
    p->block_channel = NULL;

    append(&blocked_list, p);
    dump_process_table();

    schedule(); // allow someone else to run
    unlock_scheduler();

    // in theory we'll be here when exit() will unlock us...
    // maybe the "running_proc" var already has our pointer, so we could hook some exit data there...
    p = running_process();
    klog_debug("proc_wait_child(): after scheduler scheduled, running_proc points to %s[%d] (should be the original parent)", p->name, p->pid);
    *exit_code = p->terminated_child_exit_code;
    return p->terminated_child_pid;
}

// voluntarily give up the CPU to another task
void proc_yield() {
    klog_trace("process %s is yielding", running_proc->name);
    lock_scheduler();
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// a task can ask to sleep for some time
void proc_sleep(int milliseconds) {
    if (milliseconds <= 0)
        return;
    lock_scheduler();
    process_t *p = running_process();

    // klog_trace("process %s going to sleep for %d msecs", running_proc->name, milliseconds);
    p->wake_up_time = timer_get_uptime_msecs() + milliseconds;
    p->state = BLOCKED;
    p->block_reason = SLEEPING;
    p->block_channel = NULL;

    // keep the earliest wake up time, useful for fast comparison
    next_wake_up_time = (next_wake_up_time == 0)
        ? p->wake_up_time
        : min(next_wake_up_time, p->wake_up_time);
    
    append(&blocked_list, p);
    schedule(); // allow someone else to run
    unlock_scheduler();
}

// a task can ask to be terminated
void proc_exit(int exit_code) {
    lock_scheduler();

    process_t *p = running_process();
    p->state = TERMINATED;
    p->exit_code = exit_code;
    append(&terminated_list, p);
    klog_trace("Process %s[%d] exited, exit code %d", p->name, p->pid, exit_code);

    // possibly wake up parent process
    process_t *parent = p->parent;
    if (parent != NULL && parent->state == BLOCKED && parent->block_reason == WAIT_CHILD_EXIT) {
        klog_trace("Will unblock parent process %s[%d]", parent->name, parent->pid);
        parent->terminated_child_pid = p->pid;
        parent->terminated_child_exit_code = exit_code;
        klog_debug("Added pid %d and exit code %d to parent", p->pid, exit_code);
        unblock_process(parent);
    }

    // whether we unblocked parent or not, somebody else should run
    schedule();
    unlock_scheduler();
}

pid_t proc_getpid() {
    return running_proc == NULL ? ERR_NOT_SUPPORTED : running_proc->pid;
}

pid_t proc_getppid() {
    if (running_proc == NULL)
        return ERR_NOT_SUPPORTED;
    return running_proc->parent == NULL ? 0 : running_proc->parent->pid;
}

int proc_getcwd(process_t *proc, char *buffer, int size) {
    if (size < strlen(proc->cwd_path) + 1)
        return ERR_NO_SPACE_LEFT;
    // how is this updated when the folder is deleted from the filesystem?
    // maybe a message should be broadcasted to all processes?
    strcpy(buffer, proc->cwd_path);
    return SUCCESS;
}

// in unices, this would be called chdir(), especially in libc
int proc_setcwd(process_t *proc, char *path) {

    // maybe we should resolve the path, relative to the current cwd.
    // this is to allow more of a chdir() approach
    
    // see if file exists
    file_t file;
    int err = vfs_opendir(path, &file);
    if (err) return err;

    // close previous
    if (!memchk(&proc->cwd, 0, sizeof(file_t))) {
        vfs_closedir(&proc->cwd);
        memset(&proc->cwd, 0, sizeof(file_t));
    }

    memcpy(&proc->cwd, &file, sizeof(file_t));

    if (proc->cwd_path != NULL)
        kfree(proc->cwd_path);
    proc->cwd_path = kmalloc(strlen(path) + 1);
    strcpy(proc->cwd_path, path);

    return SUCCESS;
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
    proc_exit(-7);
}

process_t *create_process(char *name, func_ptr entry_point, uint8_t priority, process_t *parent, tty_t *tty) {
    if (priority >= PROCESS_PRIORITY_LEVELS) {
        klog_warn("priority %d requested when we only have %d levels", priority, PROCESS_PRIORITY_LEVELS);
        return NULL;
    }

    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    memset(p, 0, sizeof(process_t));
    
    acquire(&pid_lock);
    p->pid = ++last_pid;
    release(&pid_lock);

    p->parent = parent;
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

    // set working directory
    proc_setcwd(p, "/");

    klog_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}

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
    
    if (!memchk(&proc->cwd, 0, sizeof(file_t)))
        vfs_closedir(&proc->cwd);
    if (proc->cwd_path != NULL)
        kfree(proc->cwd_path);
    
    // can't think of anything else to free
    kfree(proc);
}

static int allocate_file_handle(process_t *proc, file_t *file) {
    int handle = ERR_HANDLES_EXHAUSTED;

    acquire(&proc->process_lock);

    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        // if entry is all zeros, means it's unused
        if (memchk(&proc->file_handles[i], 0, sizeof(file_t))) {
            handle = i;
            break;
        }
    }

    if (handle >= 0 && handle < MAX_FILE_HANDLES)
        memcpy(&proc->file_handles[handle], file, sizeof(file_t));
    
    release(&proc->process_lock);
    return handle;
}

static bool is_valid_handle(process_t *proc, int handle) {
    if (handle < 0 || handle >= MAX_FILE_HANDLES)
        return false;
    
    // the handle should not contain all zeros, to be used
    return !memchk(&(proc->file_handles[handle]), 0, sizeof(file_t));
}

static int free_file_handle(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    
    acquire(&proc->process_lock);
    memset(&proc->file_handles[handle], 0, sizeof(file_t));
    release(&proc->process_lock);

    return SUCCESS;
}

int proc_open(process_t *proc, char *name) {
    file_t file;
    // fast resolve relative to cwd (we'll need rewinddir() at least)
    int err = vfs_open(name, &file);
    if (err) return err;

    int handle = allocate_file_handle(proc, &file);
    return handle;
}

int proc_read(process_t *proc, int handle, char *buffer, int length) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    return vfs_read(&proc->file_handles[handle], buffer, length);
}

int proc_write(process_t *proc, int handle, char *buffer, int length) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    return vfs_write(&proc->file_handles[handle], buffer, length);
}

int proc_seek(process_t *proc, int handle, int offset, enum seek_origin origin) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    return vfs_seek(&proc->file_handles[handle], offset, origin);
}

int proc_close(process_t *proc, int handle) {
    if (!is_valid_handle(proc, handle))
        return ERR_BAD_ARGUMENT;
    int err = vfs_close(&proc->file_handles[handle]);
    if (err) return err;

    free_file_handle(proc, handle);
    return SUCCESS;
}

int proc_opendir(process_t *proc, char *name) {
    file_t file;
    // fast resolve relative to cwd
    int err = vfs_opendir(name, &file);
    if (err) return err;

    int handle = allocate_file_handle(proc, &file);
    klog_trace("proc_opendir() -> %d", handle);
    klog_debug("Process handles table follows");
    klog_hex16_debug((void *)proc->file_handles, sizeof(file_t) * MAX_FILE_HANDLES, 0);
    return handle;
}

int proc_readdir(process_t *proc, int handle, dir_entry_t *entry) {
    if (handle < 0 || handle >= MAX_FILE_HANDLES)
        return ERR_BAD_ARGUMENT;
    int err = vfs_readdir(&proc->file_handles[handle], entry);
    klog_trace("proc_readdir() -> %d", err);
    return err;
}

int proc_closedir(process_t *proc, int handle) {
    if (handle < 0 || handle >= MAX_FILE_HANDLES)
        return ERR_BAD_ARGUMENT;

    int err = vfs_closedir(&proc->file_handles[handle]);
    if (err) return err;

    free_file_handle(proc, handle);
    return SUCCESS;
}



static void dump_process(process_t *proc) {
    klog_info("%-4d %-4d %-20s %08x %08x %-10s %-10s %4us", 
        proc->pid,
        proc->parent == NULL ? 0 : proc->parent->pid,
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
    klog_info("PID  PPID Name                 ESP      EIP      State      Blck Reasn    CPU");
    dump_process((process_t *)running_proc);
    for (int pri = 0; pri < PROCESS_PRIORITY_LEVELS; pri++) {
        dump_process_list(&ready_lists[pri]);
    }
    dump_process_list(&blocked_list);
    dump_process_list(&terminated_list);
}
