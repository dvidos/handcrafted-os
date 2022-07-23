#include <bits.h>
#include <errors.h>
#include <klog.h>
#include <drivers/clock.h>
#include <drivers/timer.h>
#include <multitask/process.h>
#include <devices/tty.h>
#include <filesys/vfs.h>
#include "../../libc/include/syscall.h"
#include "../../libc/include/keyboard.h"




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

static int sys_exit(uint8_t exit_code) {
    // current process exiting, preserve exit code, wake up waiting parents
    exit(exit_code);
    return 0;
}

static int sys_getkey(key_event_t *event) {
    tty_read_key(event);
    return 0;
}

int isr_syscall(struct syscall_stack stack) {
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
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_OPEN:   // arg1 = file path, returns handle or error<0
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            // find entry in process' file table
            return_value = -1;
            break;
        case SYS_READ:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_WRITE:   // arg1 = handle, arg2 = buffer, arg3 = len, returns len
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_SEEK:   // arg1 = handle, arg2 = offset, arg3 = origin, returns new position
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_CLOSE:   // arg1 = handle
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_OPEN_DIR:   // arg1 = dir path, return handle or error<0
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_READ_DIR:   // arg1 = handle, arg2 = dentry pointer
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_CLOSE_DIR:   // arg1 = handle
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_STAT:   // arg1 = path, arg2 = stat pointer
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_TOUCH:   // arg1 = path
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_MKDIR:   // arg1 = path
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_UNLINK:   // arg1 = path (dir or file)
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_GET_PID:   // returns pid
            return_value = running_process() == NULL ? ERR_NOT_SUPPORTED : (int)(running_process()->pid);
            break;
        case SYS_GET_PPID:   // returns ppid
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_FORK:   // returns 0 in child, child PID in parent, neg error in parent
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_EXEC:   // arg1 = path, arg2 = argv, arg3 = envp, returns... maybe?
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_WAIT:   // arg1 = pid
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_SLEEP:   // arg1 = millisecs
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_EXIT:   // arg1 = exit code
            return_value = sys_exit((uint8_t)stack.passed.arg1);
            break;
        case SYS_SBRK:   // arg1 = signed desired diff, returns pointer to new area
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
            break;
        case SYS_GET_UPTIME:   // returns msecs since boot (32 bits = 49 days)
            return_value = LOW_DWORD(timer_get_uptime_msecs());
            break;
        case SYS_GET_CLOCK:   // arg1 = dtime pointer
            klog_warn("Received unimplemented syscall %d", stack.passed.sysno);
            return_value = -1;
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
    
    // both positive and negative values tested and supported
    return return_value;
}
