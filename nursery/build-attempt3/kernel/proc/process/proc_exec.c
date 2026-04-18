#include "process.h"
#include "../../logger/logger.h"
#include "../../utils/panic.h"

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

    log_debug_fmt(proc_log_formatter, "after exec replace:", proc);
    
    // skip all the kernel's C callstack, and cause a return from the ISR
    interrupt_frame_t *iframe = proc_get_interrupt_frame(proc);
    log_debug("calling 'force_jump_to_user_proc(%p)' to return to user land", iframe);
    force_jump_to_user_proc(iframe);
    
    panic("exec returned, this should not happen");
    return OK;
}
