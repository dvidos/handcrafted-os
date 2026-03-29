#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_SPAWN", LOG_LEVEL_DEBUG);


int proc_spawnve(char *path, char *argv[], char *envp[]) {
    log_warn("spawnve() does not support arguments or environment yet.");
    process_t *proc;
    error_t err = process_v2_create_for_spawn(NULL, path, PRIORITY_USER_PROGRAM, &proc);
    if (err) return err;
    log_debug_fmt(proc_log_formatter, "creat2(): ", proc);

    proc_start(proc);
    return proc_get_pid(proc);
}

// loads and executes an executable in a separate process
int proc_spawn(char *path) {
    char *argv[] = { NULL };
    char *envp[] = { NULL };
    return proc_spawnve(path, argv, envp);
}
