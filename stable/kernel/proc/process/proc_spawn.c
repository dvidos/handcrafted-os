#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_SPAWN", LOG_LEVEL_INFO);


int proc_spawnve(process_t *parent, char *path, char *argv[], char *envp[]) {
    log_trace("proc_spawnve(parent=%p [pid=%d], path='%s')", parent, parent == NULL ? -1 : parent->pid, path);
    for (int i = 0; argv[i] != NULL; i++) log_trace("    argv[%d] = \"%s\";", i, argv[i]);
    for (int i = 0; envp[i] != NULL; i++) log_trace("    envp[%d] = \"%s\";", i, envp[i]);
    
    process_t *proc;
    error_t err = process_create_for_spawn(parent, path, argv, envp, PRIORITY_USER_PROGRAM, parent->vfs_ctx.mtab, &proc);
    if (err) return err;

    // log_debug_fmt(proc_log_formatter, "proc_spawnve() parent: ", parent);
    // log_debug_fmt(proc_log_formatter, "proc_spawnve() child : ", proc);

    proc_start(proc);
    return proc_get_pid(proc);
}

// loads and executes an executable in a separate process
int proc_spawn(process_t *parent, char *path) {
    log_trace("proc_spawn(parent=%p [pid=%d], path='%s')", parent, parent == NULL ? -1 : parent->pid, path);
    char *argv[] = { NULL };
    char *envp[] = { NULL };
    return proc_spawnve(parent, path, argv, envp);
}
