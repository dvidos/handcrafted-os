#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h> // For pid_t

// Options for waitpid
#define WNOHANG    1    /* Don't wait if no status is available. */
#define WUNTRACED  2    /* Report stopped child, too. */
#define WCONTINUED 8    /* Report continued child, too. */

// Function prototypes
pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);

// Status analysis macros
#define __WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define __WSTOPSIG(status)    __WEXITSTATUS(status)
#define __WTERMSIG(status)    ((status) & 0x7F)
#define __WCOREDUMP(status)   ((status) & 0x80)

#define WIFEXITED(status)     (__WTERMSIG(status) == 0)
#define WEXITSTATUS(status)   __WEXITSTATUS(status)
#define WIFSIGNALED(status)   (__WTERMSIG(status) != 0 && __WSTOPSIG(status) == 0)
#define WTERMSIG(status)      __WTERMSIG(status)
#define WIFSTOPPED(status)    ((status) & 0xFF) == 0x7F
#define WSTOPSIG(status)      __WSTOPSIG(status)
#define WIFCONTINUED(status)  ((status) == 0xFFFF)

#endif /* _SYS_WAIT_H */