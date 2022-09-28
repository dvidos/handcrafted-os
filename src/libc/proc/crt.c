#include <ctypes.h>
#include <syscall.h>
#include <stdlib.h>


/**
 * As at the moment I have no idea how I will exec() any user land process,
 * I am atm just interested in removing the warnings and improving the build system
 * 
 * See also https://wiki.osdev.org/Creating_a_C_Library#Program_Initialization
 * 
 * Update: three months later, we are loading an ELF into memory,
 * looking to jump into the _start address.
 * 
 * Now we know that the crt0 is supposed to implement the ABI (Application Binary Interface)
 * and is processor and system specific.
 * 
 * It's work is to setup what's provided by the kernel, in a way that the C library and program
 * expect them. For example, argv, argc, envp, etc. Finally, after main exists,
 * it calls _exit to run the atexit() methods and then terminate the process.
 * 
 * See also https://handwiki.org/wiki/Crt0
 */

// arguments are pushed on the new stack, before jumping here
// see load_and_run_executable() method in kernel
void _start(int argc, char **argv, char **envp) {

    extern void __init_heap();
    __init_heap();

    // then call main()

    extern int main(int argc, char *argv[], char *envp[]);
    int exit_code = main(argc, argv, envp);

    // then call the atexit() functions,
    // we could call terminate() to remove our process from the running / ready list
    exit(exit_code);
}

