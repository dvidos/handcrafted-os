#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errors.h>
#include <klog.h>
#include <multitask/process.h>
#include <devices/tty.h>




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


void sys_puts(char *message) {
    process_t *proc = running_process();
    if (proc != NULL && proc->tty != NULL) {
        tty_write(message);
        tty_write("\n");
    }
}

int isr_syscall(struct syscall_stack stack) {
    // it seems we are in the stack of the user process
    int err = SUCCESS;
    
    switch (stack.passed.sysno) {
        case 1:
            sys_puts((char *)stack.passed.arg1);
            break;
        default:
            klog_warn("Received syscall interrupt!");
            klog_debug("  sysno = %d (eax)", stack.passed.sysno);
            klog_debug("  arg1  = %d (0x%08x) (ebx)", stack.passed.arg1, stack.passed.arg1);
            klog_debug("  arg2  = %d (0x%08x) (ecx)", stack.passed.arg2, stack.passed.arg2);
            klog_debug("  arg3  = %d (0x%08x) (edx)", stack.passed.arg3, stack.passed.arg3);
            klog_debug("  arg4  = %d (0x%08x) (esi)", stack.passed.arg4, stack.passed.arg4);
            klog_debug("  arg5  = %d (0x%08x) (edi)", stack.passed.arg5, stack.passed.arg5);
            err = stack.passed.sysno + 5;
            break;
    }
    
    // both positive and negative values tested and supported
    return err;
}
