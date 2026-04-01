#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_EXEC", LOG_LEVEL_TRACE);


// replace this process with the executable passed in.
int proc_execve(process_t *proc, const char *path, char *const argv[], char *const envp[]) {
    log_trace("proc_execve(proc=%p [pid=%d], path='%s')", proc, proc == NULL ? -1 : proc->pid, path);

    return ERR_NOT_IMPLEMENTED;
}

