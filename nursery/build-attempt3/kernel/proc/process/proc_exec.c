#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_EXEC", LOG_LEVEL_TRACE);


// replace this process with the executable passed in.
int proc_execve(process_t *proc, const char *path, char *argv[], char *envp[]) {
    log_trace("proc_execve(proc=%p [pid=%d], path='%s')", proc, proc == NULL ? -1 : proc->pid, path);
    for (int i = 0; argv[i] != NULL; i++) log_trace("    argv[%d] = \"%s\";", i, argv[i]);
    for (int i = 0; envp[i] != NULL; i++) log_trace("    envp[%d] = \"%s\";", i, envp[i]);
    
    log_debug_fmt(proc_log_formatter, "before exec replace:", proc);

    error_t err = process_v2_replace_for_exec(proc, path, argv, envp);
    if (err) return err;

    log_debug_fmt(proc_log_formatter, "after exec replace:", proc);

    return OK;
}

