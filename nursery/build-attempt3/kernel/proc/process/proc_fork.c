#include "process.h"
#include "../../logger/logger.h"

MODULE("PROC_FORK", LOG_LEVEL_TRACE);


// clone, return child's PID on parent, zero on child.
int proc_fork(process_t *proc) {
    log_trace("proc_fork(proc=%p)", proc);

    // need to duplicate stack and heap, memory pages, file handles,
    // then even copy the trap state (e.g. the actual EIP value)
    // to resume exactly where we left off.
    // but also open files and other stuff.

    /*
        Ok, some learnings:
        - at some point we shall need to write a process memory blocks
          with code or data, either from the ELF, or from the parent process.
        - this can be done in two ways:
          a. the kernel maintains a permanent mapping of pages in the upper virtual memory
             (e.g. at 3.5GB) and uses that area to map different physical pages, 
             so it can write to them. They are later remapped to the child's proc space.
          b. the kernel allocates a single page (or just a few) to load through that
             all the data needed, again later mapping them to the child process.
        - in all cases, the "supervisor bit" is set for these pages, making them 
          safe for kernel, but inaccessible by the user processes.
        - the various R/E/X flags should also be set, depending on the usage,
          when mapping these pages to the final process.
        - finaly, when we have mapped and prepared all memory of the child,
          we copy the stack snapshot, file descriptors, 
    */

    return ERR_NOT_IMPLEMENTED;
}
