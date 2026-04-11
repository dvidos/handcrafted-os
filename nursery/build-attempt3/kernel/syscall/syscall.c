#include "../include/bits.h"
#include "../arch/cpu.h"
#include "../include/uapi/errors.h"
#include "../logger/logger.h"
#include "../drivers/clock.h"
#include "../drivers/timer.h"
#include "../proc/process/process.h"
#include "../klib/string.h"
#include "../memory/vmm.h"

#include "../include/uapi/syscall.h"
#include "../include/uapi/key_event.h"
#include "../include/uapi/time.h"

MODULE("SYSCALL", LOG_LEVEL_WARN);


// in this file we freely convert from u32 to pointers and vice versa
_Static_assert(sizeof(uint32_t) == sizeof(void *));


static void sys_log_entry(process_t *proc, int level, uint8_t *buffer) {
    char prefix[16];
    sprintfn(prefix, sizeof(prefix), "SYSLOG", proc->pid);

    // syslog is a userland deamon, we need to drop this functionality.
    logger_append(prefix, level, "%s[%d]  %s", proc->name, proc->pid, buffer);
}

static void sys_log_hex(process_t *proc, int level, uint8_t *address, uint32_t length, uint32_t starting_num) {
    log_debug_hex(address, length, starting_num);
}

static virt_addr_t sys_sbrk(int difference) {
    process_t *p = running_process();
    if (!p) return 0;
    
    virt_addr_t original_break = p->memory.user_heap.address + p->memory.user_heap.size;

    if (difference > 0) {
        difference = vmm_round_up(difference);
        virt_addr_t new_break = p->memory.user_heap.address + p->memory.user_heap.size;
        vmm_allocate_memory_range_this_pd(new_break, new_break + difference);
        p->memory.user_heap.size += difference;
    }

    return original_break;
}
static int sys_exit(int exit_code) {
    proc_exit(running_process(), exit_code);
    return 0;
}
static int sys_sleep(uint32_t milliseconds) {
    proc_sleep(running_process(), milliseconds);
    return 0;
}
static int sys_yield() {
    proc_yield(running_process());
    return 0;
}
static int sys_get_cwd(char *buffer, int length) {
    return proc_getcwd(running_process(), buffer, length);
}
static int sys_chdir(char *path) {
    return proc_chdir(running_process(), path);
}
static int sys_open(char *path) {
    return proc_open(running_process(), path);
}
static int sys_read(int handle, char *buffer, int length) {
    return proc_read(running_process(), handle, buffer, length);
}
static int sys_write(int handle, char *buffer, int length) {
    return proc_write(running_process(), handle, buffer, length);
}
static int sys_seek(int handle, int offset, int origin) {
    return proc_seek(running_process(), handle, offset, origin);
}
static int sys_close(int handle) {
    return proc_close(running_process(), handle);
}
static int sys_opendir(char *path) {
    return proc_opendir(running_process(), path);
}
static int sys_rewinddir(int handle) {
    return proc_rewinddir(running_process(), handle);
}
// static int sys_readdir(int handle, dirent_t *dirent) {
static int sys_readdir(int handle, void *dirent) {
    return proc_readdir(running_process(), handle, dirent);
}
static int sys_closedir(int handle) {
    return proc_closedir(running_process(), handle);
}
static int sys_dup(int fd) {
    return proc_dup(running_process(), fd);    
}
static int sys_dup2(int fd1, int fd2) { 
    return proc_dup2(running_process(), fd1, running_process(), fd2);
}
static int sys_pipe(int fds[]) {
    return proc_pipe(running_process(), fds);
}
static int sys_fork() {
    return proc_fork(running_process());
}
static int sys_exec(char *path, char **argv, char **envp) {
    return proc_execve(running_process(), path, argv, envp);
}
static int sys_spawn(char *path, char **argv, char **envp) {
    return proc_spawnve(running_process(), path, argv, envp);
}
static int sys_wait_any_child(int *exit_code) {
    return proc_wait(running_process(), exit_code);
}
static int sys_wait_spec_child(pid_t child_pid, int *exit_code, int mode) {
    return proc_waitpid(running_process(), child_pid, exit_code, mode);
}
static int sys_get_clocktime(clocktime_t *ct) {

    // translate RTC info structure into libc structure
    real_time_clock_info_t rtc;
    get_real_time_clock(&rtc);

    ct->years = rtc.years;
    ct->months = rtc.months;
    ct->days = rtc.days;
    ct->dow = rtc.dow;
    ct->hours = rtc.hours;
    ct->minutes = rtc.minutes;
    ct->seconds = rtc.seconds;

    return OK;
}
int sys_uptime(uint64_t *msecs) {
    *msecs = timer_get_uptime_msecs();
    return OK;
}


void isr_syscall(trap_frame_t *tf) {
    
    /* before getting to this function, the assembly isr handler
       has pushed CS, DS and SS into the stack, and will subsequently
       pop the values from the stack. we had 
       cases where the SS value could be overwritten and this caused
       a General Protection Fault, as there was no such Segment Descriptor. 
       Therefore, we hope to catch any clobbed stack cases */
    #define STACK_GUARD_MAGIC_NUMBER   0x1BADCAFE
    volatile int stack_guard = STACK_GUARD_MAGIC_NUMBER;


    // it seems we are in the stack of the user process
    int return_value = 0;
    uint32_t arg0 = tf->eax; // usuallys the sysno
    uint32_t arg1 = tf->ebx;
    uint32_t arg2 = tf->ecx;
    uint32_t arg3 = tf->edx;
    uint32_t arg4 = tf->esi;
    uint32_t arg5 = tf->edi;

    switch (arg0) {
        case SYS_ECHO_TEST:
            return_value = arg1;
            break;
        case SYS_ADD_TEST:
            return_value = arg1 + arg2 + arg3 +
                arg4 + arg5;
            break;

        case SYS_LOG_ENTRY:
            sys_log_entry(running_process(), arg1, (uint8_t *)arg2);
            break;
        case SYS_LOG_HEX_DUMP:
            sys_log_hex(running_process(), arg1, (uint8_t *)arg2, (uint32_t)arg3, (uint32_t)arg4);
            break;
        case SYS_GET_CWD: // arg1 = buffer, arg2 = buffer len
            return_value = sys_get_cwd((char *)arg1, arg2);
            break;
        case SYS_CHDIR: // arg1 = path
            return_value = sys_chdir((char *)arg1);
            break;
        case SYS_OPEN:   // arg1 = file path, returns handle or error<0
            return_value = sys_open((char *)arg1);
            break;
        case SYS_READ:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            return_value = sys_read(arg1, (char *)arg2, arg3);
            break;
        case SYS_WRITE:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            return_value = sys_write(arg1, (char *)arg2, arg3);
            break;
        case SYS_SEEK:   // arg1 = handle, arg2 = offset, arg3 = origin, returns new position
            return_value = sys_seek(arg1, arg2, arg3);
            break;
        case SYS_CLOSE:   // arg1 = handle
            return_value = sys_close(arg1);
            break;
        case SYS_OPEN_DIR:   // arg1 = dir path, return handle or error<0
            return_value = sys_opendir((char *)arg1);
            break;
        case SYS_READ_DIR:   // arg1 = handle, arg2 = dentry pointer
            return_value = sys_readdir(arg1, (void *)arg2);
            break;
        case SYS_CLOSE_DIR:   // arg1 = handle
            return_value = sys_closedir(arg1);
            break;
        case SYS_TOUCH:   // arg1 = path
            // return_value = vfs_touch((char *)arg1);
            break;
        case SYS_UNLINK:   // arg1 = path (dir or file)
            return_value = vfs_unlink((char *)arg1);
            break;
        case SYS_MKDIR:   // arg1 = path
            return_value = vfs_mkdir((char *)arg1);
            break;
        case SYS_RMDIR:  // arg1 = path
            return_value = vfs_rmdir((char *)arg1);
            break;
        case SYS_DUP:
            return_value = sys_dup(arg1);
            break;
        case SYS_DUP2:
            return_value = sys_dup2(arg1, arg2);
            break;
        case SYS_PIPE:
            // arg1 is an array of two integers ?!?
            return_value = sys_pipe((int*)arg1);
            break;
        case SYS_GET_PID:   // returns pid
            return_value = proc_get_pid(running_process());
            break;
        case SYS_GET_PPID:   // returns ppid
            return_value = proc_get_ppid(running_process());
            break;
        case SYS_FORK:   // returns 0 in child, child PID in parent, neg error in parent
            return_value = sys_fork();
            break;
        case SYS_EXEC:   // arg1 = path, arg2 = argv, arg3 = envp, returns... maybe?
            return_value = sys_exec((char *)arg1, (char **)arg2, (char **)arg3);
            break;
        case SYS_SPAWN:   // arg1 = path, arg2 = argv, arg3 = envp, returns... maybe?
            return_value = sys_spawn((char *)arg1, (char **)arg2, (char **)arg3);
            break;
        case SYS_WAIT_ANY_CHILD:
            return_value = sys_wait_any_child((int *)arg1);
            break;
        case SYS_WAIT_SPEC_CHILD:
            return_value = sys_wait_spec_child((pid_t)arg1, (int *)arg2, arg3);
            break;
        case SYS_YIELD:
            return_value = sys_yield();
            break;
        case SYS_SLEEP:   // arg1 = millisecs
            return_value = sys_sleep((uint32_t)arg1);
            break;
        case SYS_EXIT:   // arg1 = exit code
            return_value = sys_exit(arg1);
            break;
        case SYS_SBRK:   // arg1 = signed desired diff, returns pointer to new area
            return_value = (int)sys_sbrk(arg1);
            break;
        case SYS_GET_UPTIME:   // returns msecs since boot (32 bits = 49 days)
            sys_uptime((uint64_t *)arg1);
            break;
        case SYS_GET_CLOCK:   // arg1 = clocktime pointer
            sys_get_clocktime((clocktime_t *)arg1);
            break;
        default:
            log_warn("Received syscall interrupt!");
            log_debug("  sysno = %d (eax)", arg0);
            log_debug("  arg1  = %d (0x%08x) (ebx)", arg1, arg1);
            log_debug("  arg2  = %d (0x%08x) (ecx)", arg2, arg2);
            log_debug("  arg3  = %d (0x%08x) (edx)", arg3, arg3);
            log_debug("  arg4  = %d (0x%08x) (esi)", arg4, arg4);
            log_debug("  arg5  = %d (0x%08x) (edi)", arg5, arg5);
            break;
    }
    
    if (stack_guard != STACK_GUARD_MAGIC_NUMBER) {
        log_critical("Syscall garbled stack detected! Stack dump follows, from guard downwards");
        log_debug_hex((void *)&stack_guard, 16 * 16, (uint32_t)&stack_guard);
    }

    // both positive and negative values tested and supported
    tf->eax = return_value;
}
