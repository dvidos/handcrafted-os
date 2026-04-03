#include <ctypes.h>
#include <syscall.h>
#include <stdlib.h>

extern void __init_heap();
extern void __init_env(char **envp);
extern int main(int argc, char *argv[], char *envp[]);


void __libc_init(int argc, char **argv, char **envp) {
    // syslog_trace("__libc_init(%d, %p, %p) running...", argc, argv, envp);

    __init_heap();
    __init_env(envp);

    int exit_code = main(argc, argv, envp);

    exit(exit_code);
}

