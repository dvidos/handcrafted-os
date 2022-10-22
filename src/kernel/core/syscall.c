#include <bits.h>
#include <cpu.h>
#include <errors.h>
#include <klog.h>
#include <drivers/clock.h>
#include <drivers/timer.h>
#include <multitask/process.h>
#include <multitask/exec.h>
#include <devices/tty.h>
#include <filesys/vfs.h>
#include <memory/virtmem.h>

#include "../../libc/include/syscall.h"
#include "../../libc/include/keyboard.h"
#include "../../libc/include/time.h"

MODULE("SYSCALL");

#define STACK_GUARD_MAGIC_NUMBER   0x1BADCAFE


// things pushed in the isr0x80 we have in assembly appear as arguments here
// see isr0x80 in idt_low.asm for how things are pushed
struct syscall_stack
{
    union {
        struct {
            uint32_t original_ds;  // ignore
            uint32_t arg5, arg4, arg3, arg2, arg1, sysno;
        } passed;
        struct {
            uint32_t dword[12];
        } uniform;
    };
};


static int sys_puts(char *message) {
    // we don't write a "\n" per se. we don't like to. there.
    tty_write(message);
    return 0;
}

static int sys_putchar(int c) {
    char buff[2];
    buff[0] = (char)c;
    buff[1] = '\0';
    tty_write(buff);
    return 0;
}

static int sys_clear_screen() {
    tty_clear();
    return 0;
}

static int sys_where_xy(int *x, int *y) {
    uint8_t row, col;
    tty_get_cursor(&row, &col);
    *x = col;
    *y = row;
    return 0;
}

static int sys_goto_xy(int x, int y) {
    tty_set_cursor(y, x);
    return 0;
}

static int sys_screen_dimensions(int *cols, int *rows) {
    tty_get_dimensions(rows, cols);
    return 0;
}

static int sys_get_screen_color() {
    return tty_get_color();
}

static int sys_set_screen_color(int color) {
    tty_set_color(color);
    return 0;
}

static int sys_getkey(key_event_t *event) {
    tty_read_key(event);
    return 0;
}

static void sys_log_entry(int level, uint8_t *buffer) {
    klog_user_syslog(level, buffer);
}

static void sys_log_hex(int level, uint8_t *address, uint32_t length, uint32_t starting_num) {
    klog_debug_hex(address, length, starting_num);
}

static void *sys_sbrk(int diff_size) {
    process_t *p = running_process();
    if (p == NULL)
        return NULL;
    void *initial_break = p->user_proc.heap + p->user_proc.heap_size;
    if (diff_size > 0) {
        diff_size = (diff_size + 0xFFF) & 0xFFFFF000; // round up to next page / 4K
        void *heap_end = p->user_proc.heap + p->user_proc.heap_size;
        allocate_virtual_memory_range(heap_end, heap_end + diff_size, p->page_directory);
        p->user_proc.heap_size += diff_size;
    }
    return initial_break;
}
static int sys_exit(int exit_code) {
    // current process exiting, preserve exit code, wake up waiting parents
    proc_exit(exit_code);
    return 0;
}
static int sys_sleep(uint32_t milliseconds) {
    proc_sleep(milliseconds);
    return 0;
}
static int sys_yield() {
    proc_yield();
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
static int sys_seek(int handle, int offset, enum seek_origin origin) {
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
static int sys_readdir(int handle, dirent_t *dirent) {
    return proc_readdir(running_process(), handle, dirent);
}
static int sys_closedir(int handle) {
    return proc_closedir(running_process(), handle);
}
static int sys_exec(char *path, char **argv, char **envp) {
    return execve(path, argv, envp);
}
static int sys_wait_child(int *exit_code) {
    return proc_wait_child(exit_code);
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

    return SUCCESS;
}

int isr_syscall(struct syscall_stack stack) {
    /* before getting to this function, the assembly isr handler
       has pushed CS, DS and SS into the stack, and will subsequently
       pop the values from the stack. we had 
       cases where the SS value could be overwritten and this caused
       a General Protection Fault, as there was no such Segment Descriptor. 
       Therefore, we hope to catch any clobbed stack cases */
    volatile int stack_guard = STACK_GUARD_MAGIC_NUMBER;
    
    // it seems we are in the stack of the user process
    int return_value = 0;

    switch (stack.passed.sysno) {
        case SYS_ECHO_TEST:
            return_value = stack.passed.arg1;
            break;
        case SYS_ADD_TEST:
            return_value = stack.passed.arg1 + stack.passed.arg2 + stack.passed.arg3 +
                stack.passed.arg4 + stack.passed.arg5;
            break;

        case SYS_LOG_ENTRY:
            sys_log_entry(stack.passed.arg1, (uint8_t *)stack.passed.arg2);
            break;
        case SYS_LOG_HEX_DUMP:
            sys_log_hex(stack.passed.arg1, (uint8_t *)stack.passed.arg2, (uint32_t)stack.passed.arg3, (uint32_t)stack.passed.arg4);
            break;
        case SYS_PUTS:   // arg1 = string
            return_value = sys_puts((char *)stack.passed.arg1);
            break;
        case SYS_PUTCHAR:   // arg1 = char
            return_value = sys_putchar(stack.passed.arg1);
            break;
        case SYS_CLEAR_SCREEN:   // no args
            return_value = sys_clear_screen();
            break;
        case SYS_WHERE_XY:   // arg1 = *x, arg2 = *y
            return_value = sys_where_xy((int *)stack.passed.arg1, (int *)stack.passed.arg2);
            break;
        case SYS_GOTO_XY:   // arg1 = x, arg2 = y, zero based
            return_value = sys_goto_xy(stack.passed.arg1, stack.passed.arg2);
            break;
        case SYS_SCREEN_DIMENSIONS:   // arg1 = *cols, arg2 = *rows
            return_value = sys_screen_dimensions((int *)stack.passed.arg1, (int *)stack.passed.arg2);
            break;
        case SYS_GET_SCREEN_COLOR:
            return_value = sys_get_screen_color();
            break;
        case SYS_SET_SCREEN_COLOR:
            return_value = sys_set_screen_color(stack.passed.arg1);
            break;
        case SYS_GET_KEY_EVENT:   // returns... a lot of info (we have 4 bytes)
            return_value = sys_getkey((key_event_t *)stack.passed.arg1);
            break;
        case SYS_GET_MOUSE_EVENT:   // returns... a lot of info (we have 4 bytes)
            return_value = ERR_NOT_SUPPORTED;
            break;
        case SYS_GET_CWD: // arg1 = buffer, arg2 = buffer len
            return_value = sys_get_cwd((char *)stack.passed.arg1, stack.passed.arg2);
            break;
        case SYS_CHDIR: // arg1 = path
            return_value = sys_chdir((char *)stack.passed.arg1);
            break;
        case SYS_OPEN:   // arg1 = file path, returns handle or error<0
            return_value = sys_open((char *)stack.passed.arg1);
            break;
        case SYS_READ:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            return_value = sys_read(stack.passed.arg1, (char *)stack.passed.arg2, stack.passed.arg3);
            break;
        case SYS_WRITE:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            return_value = sys_write(stack.passed.arg1, (char *)stack.passed.arg2, stack.passed.arg3);
            break;
        case SYS_SEEK:   // arg1 = handle, arg2 = offset, arg3 = origin, returns new position
            return_value = sys_seek(stack.passed.arg1, stack.passed.arg2, (enum seek_origin)stack.passed.arg3);
            break;
        case SYS_CLOSE:   // arg1 = handle
            return_value = sys_close(stack.passed.arg1);
            break;
        case SYS_OPEN_DIR:   // arg1 = dir path, return handle or error<0
            return_value = sys_opendir((char *)stack.passed.arg1);
            break;
        case SYS_READ_DIR:   // arg1 = handle, arg2 = dentry pointer
            return_value = sys_readdir(stack.passed.arg1, (dirent_t *)stack.passed.arg2);
            break;
        case SYS_CLOSE_DIR:   // arg1 = handle
            return_value = sys_closedir(stack.passed.arg1);
            break;
        case SYS_TOUCH:   // arg1 = path
            return_value = vfs_touch((char *)stack.passed.arg1);
            break;
        case SYS_UNLINK:   // arg1 = path (dir or file)
            return_value = vfs_unlink((char *)stack.passed.arg1);
            break;
        case SYS_MKDIR:   // arg1 = path
            return_value = vfs_mkdir((char *)stack.passed.arg1);
            break;
        case SYS_RMDIR:  // arg1 = path
            return_value = vfs_rmdir((char *)stack.passed.arg1);
            break;
        case SYS_GET_PID:   // returns pid
            return_value = proc_getpid();
            break;
        case SYS_GET_PPID:   // returns ppid
            return_value = proc_getppid();
            break;
        case SYS_FORK:   // returns 0 in child, child PID in parent, neg error in parent
            return_value = proc_fork();
            break;
        case SYS_EXEC:   // arg1 = path, arg2 = argv, arg3 = envp, returns... maybe?
            return_value = sys_exec((char *)stack.passed.arg1, (char **)stack.passed.arg2, (char **)stack.passed.arg3);
            break;
        case SYS_WAIT_CHILD:
            return_value = sys_wait_child((int *)stack.passed.arg1);
            break;
        case SYS_YIELD:
            return_value = sys_yield();
            break;
        case SYS_SLEEP:   // arg1 = millisecs
            return_value = sys_sleep((uint32_t)stack.passed.arg1);
            break;
        case SYS_EXIT:   // arg1 = exit code
            return_value = sys_exit(stack.passed.arg1);
            break;
        case SYS_SBRK:   // arg1 = signed desired diff, returns pointer to new area
            return_value = (int)sys_sbrk(stack.passed.arg1);
            break;
        case SYS_GET_UPTIME:   // returns msecs since boot (32 bits = 49 days)
            timer_get_uptime_msecs((uint64_t *)stack.passed.arg1);
            break;
        case SYS_GET_CLOCK:   // arg1 = clocktime pointer
            sys_get_clocktime((clocktime_t *)stack.passed.arg1);
            break;
        default:
            klog_warn("Received syscall interrupt!");
            klog_debug("  sysno = %d (eax)", stack.passed.sysno);
            klog_debug("  arg1  = %d (0x%08x) (ebx)", stack.passed.arg1, stack.passed.arg1);
            klog_debug("  arg2  = %d (0x%08x) (ecx)", stack.passed.arg2, stack.passed.arg2);
            klog_debug("  arg3  = %d (0x%08x) (edx)", stack.passed.arg3, stack.passed.arg3);
            klog_debug("  arg4  = %d (0x%08x) (esi)", stack.passed.arg4, stack.passed.arg4);
            klog_debug("  arg5  = %d (0x%08x) (edi)", stack.passed.arg5, stack.passed.arg5);
            break;
    }
    
    if (stack_guard != STACK_GUARD_MAGIC_NUMBER) {
        klog_crit("Syscall garbled stack detected! Stack dump follows, from guard downwards");
        klog_debug_hex((void *)&stack_guard, 16 * 16, (uint32_t)&stack_guard);
    }

    // both positive and negative values tested and supported
    return return_value;
}
