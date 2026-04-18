#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_EXEC", LOG_LEVEL_TRACE);

extern void force_jump_to_user_proc(interrupt_frame_t *iframe);



// replace this process with the executable passed in.
int proc_execve(process_t *proc, const char *path, char *argv[], char *envp[]) {
    log_trace("proc_execve(proc=%p [pid=%d], path='%s')", proc, proc == NULL ? -1 : proc->pid, path);
    for (int i = 0; argv && argv[i]; i++) log_trace("    argv[%d] = \"%s\";", i, argv[i]);
    for (int i = 0; envp && envp[i]; i++) log_trace("    envp[%d] = \"%s\";", i, envp[i]);
    
    log_debug_fmt(proc_log_formatter, "before exec replace:", proc);

    error_t err = process_replace_for_exec(proc, path, argv, envp);
    if (err) return err;

    interrupt_frame_t *iframe = proc_get_interrupt_frame(proc);
    iframe->edi = 0x11111111;
    iframe->esi = 0x22222222;
    iframe->ebp = 0x33333333;
    iframe->ebx = 0x44444444;
    iframe->edx = 0x55555555;
    iframe->ecx = 0x66666666;
    iframe->eax = 0x77777777;
    
    log_debug_fmt(proc_log_formatter, "after exec replace:", proc);
    uint32_t user_stack_top = proc->memory.user_stack.address + proc->memory.user_stack.size;
    uint32_t user_esp = iframe->user_esp;
    uint32_t length = user_stack_top - user_esp;
    log_debug("User stack dump (user_esp=%p, user_stack_top=%p)", user_esp, user_stack_top);
    log_debug_hex((void *)user_esp, length, user_esp);

    // uint32_t *raw = (uint32_t*)iframe;
    log_debug("Frame EAX at %p is %p", &iframe->eax, iframe->eax);
    log_debug("Frame EIP at %p is %p", &iframe->eip, iframe->eip);
    log_debug("Frame ESP at %p is %p", &iframe->user_esp, iframe->user_esp);
    
    uint32_t *ustack = (uint32_t*)0x7fffffd0; 
    log_debug("U-Stack Content: [0]=%p, [argc]=%d, [argv]=%p, [envp]=%p", ustack[0], ustack[1], ustack[2], ustack[3]);


    // skip all the kernel's C callstack, and cause a return from the ISR
    log_debug("calling 'force_jump_to_user_proc(%p)' to return to user land", iframe);
    force_jump_to_user_proc(iframe);
    log_error("we should not get here...");

    return OK;
}
