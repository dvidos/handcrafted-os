#include "../include/uapi/errors.h"
#include "../utils/logger.h"
#include "../klib/string.h"
// #include "../filesys/vfs.h"
#include "process.h"
#include "../klib/strvec.h"
#include "../devices/tty.h"
#include "elf_loader.h"
#include "../memory/virtmem.h"
#include "../memory/kheap.h"

MODULE("EXEC", LOG_LEVEL_WARN);


// try to keep a balance of executables-based processes, and light weight threads.

/**
 * When we execute a file,
 * 
 * we shall load and parse the ELF header,
 * we shall allocate physical memory for the code, data, bss, heap, stack areas
 * we shall load the various areas into memory,
 * we shall create a stack entry, by pushing the argv[] and maybe the environment
 * (as if someone had called the main function)
 * we shall put this process into the ready queue, for next scheduling.
 * 
 * Notice, when we go to load the file, there's an opportunity for a shebang.
 * If the first two bytes are "#!" and a valid program follows (e.g. /bin/python)
 * then we can execute the program, passing the filename as the first argument.
 * 
 * Also notice that in unix, exec() does not allocate a new process, this is done via fork() only.
 * I do not remember why this is, but I read the reason in one of the pdfs.
 * 
 * Also, in traditional unix, exec does not return in case of success. 
 * 
 * We create a new process. If redirection or other things are wanted, we'll see.
 */

static void load_and_run_executable();



// return pid for success, negative value for errors
int execve(char *path, char *argv[], char *envp[]) {
    return ERR_NOT_IMPLEMENTED;
//     log_trace("execve(path=\"%s\")", path);
//     open_file_t *file = NULL;
//     int err;
//     bool file_open = false;

//     err = vfs_open(path, &file);
//     if (err) goto exit;
//     file_open = true;

//     // this is where we could support the "#!/bin/sh" construct
//     err = verify_elf_executable(file);
//     if (err) goto exit;
    
//     err = vfs_close(file);
//     if (err) goto exit;
//     file_open = false;

//     process_t *parent = running_process();
//     process_t *new_proc = create_process(
//         path,
//         load_and_run_executable,
//         parent == NULL ? PRIORITY_USER_PROGRAM : parent->priority,
//         parent,
//         parent == NULL ? NULL : parent->tty
//     );

//     log_debug("Process %s[%d] created process %s[%d] for executing",
//         parent->name,
//         parent->pid,
//         new_proc->name,
//         new_proc->pid
//     );

//     // make child have the same working directory as parent
//     // this allows loading execs without the full path
//     proc_chdir(new_proc, parent->curr_dir_path);

//     // we need to populate the process with enough data to be able to start.
//     // 
//     // in traditional unix, and today's (2022) linux, it seems that
//     // the actual table of argv/envp pointers is created (pushed) on the stack,
//     // and we pass pointers to the stack locations of the first element
//     // to the main() function.
//     // 
//     // maybe the inital idea was to push the actual string pointers on the stack,
//     // as actual parameters to the main() function, and then it evolved 
//     // into passing the address of the array of the pointers...
//     // 
//     // maybe the same was used to pass the initial environment.
//     // from then on, in-process environment changes are managed in libc,
//     // where the "char **environ" variable will change value every time
//     // we need to allocate a new environment block.
    
//     new_proc->user_proc.executable_path = kmalloc(strlen(path) + 1);
//     strcpy(new_proc->user_proc.executable_path, path);
//     new_proc->user_proc.argv = clone_strvec(argv);
//     new_proc->user_proc.envp = clone_strvec(envp);

//     // not much left, cheers!
//     log_info("execve(): starting process %s[%d]", new_proc->name, new_proc->pid);
//     start_process(new_proc);

//     // usually here we have the parent as current process (e.g. vi was launched, we go sh as current)
//     log_debug("execve(): after start_process() returned, current proc is %s[%d]", running_process()->name, running_process()->pid);

//     err = OK;
// exit:
//     if (file_open)
//         vfs_close(file);
//     if (err)
//         log_debug("execve() --> %d", err);
//     return err == OK ? new_proc->pid : err;
}


// loads and executes an executable in a separate process
int exec(char *path) {
    // both argv and envp are considered to be terminated by a null ptr.
    char *null_ptr = NULL;
    char *argv[2] = { path, NULL };

    // we should reuse current process' environment
    return execve(path, argv, &null_ptr);
}

static void load_and_run_executable() {
    // int err;
    // process_t *proc = running_process();

    // // grab info from proc, load the executable, jump to start.
    // log_debug("load_and_run_executable() running");

    // // find info from the file
    // open_file_t *file = NULL;
    // err = vfs_open(proc->user_proc.executable_path, &file);
    // if (err) {
    //     log_error("Failed opening executable \"%s\"", proc->user_proc.executable_path);
    //     proc_exit(-1);
    // }

    // // gather information from the elf file
    // void *virt_addr_start = NULL;
    // void *virt_addr_end = NULL;
    // void *elf_entry_point = NULL;

    // err = get_elf_load_information(file, &virt_addr_start, &virt_addr_end, &elf_entry_point);
    // log_debug("ELF to be loaded at virtual addresses 0x%p - 0x%x, entry point 0x%p", virt_addr_start, virt_addr_end, elf_entry_point);
    // if (err) {
    //     log_error("Failed getting info from executable");
    //     proc_exit(-2);
    // }

    // // memory map of a process:
    // // 1-4 MB: kernel
    // // 128 MB entry point and parts of the elf file (data, rodata, bss, text)
    // // below that, stack, growing downwards
    // // above that, heap, expandable

    // // stack will be located below the executable loaded address (and be 16 bytes aligned)
    // int stack_size = 256 * 1024;
    // void *stack_bottom = (void *)(((uint32_t)virt_addr_start - stack_size - 4096) & 0xFFFFF000);

    // // heap will be located above the executable loaded address, growing up
    // int heap_size = 0;
    // void *heap = (void *)(((uint32_t)virt_addr_end + 0xFFF) & 0xFFFFF000);

    // // create something to load the segments (kernel mapped included)
    // void *page_directory = create_page_directory(true);
    // allocate_virtual_memory_range(stack_bottom, heap + heap_size, page_directory);
    // log_debug("Allocated new page directory 0x%x for execve()", page_directory);
    // dump_page_directory(page_directory);
    // // dump_page_directory(get_kernel_page_directory());

    // // we are not waiting for a switch, we have to set CR3 now, to load the file.
    // proc->page_directory = page_directory;
    // set_page_directory_register(page_directory);

    // // we should be safe to do it now
    // err = load_elf_into_memory(file);
    // if (err) {
    //     log_error("Failed loading executable \"%s\"", proc->user_proc.executable_path);
    //     proc_exit(-3);
    // }

    // err = vfs_close(file);
    // if (err) {
    //     log_error("Failed closing executable \"%s\"", proc->user_proc.executable_path);
    //     proc_exit(-4);
    // }

    // // allow libc to use the heap.
    // proc->user_proc.heap = heap;
    // proc->user_proc.heap_size = heap_size;
    
    // proc->user_proc.stack_size = stack_size;
    // proc->user_proc.stack_bottom = stack_bottom;
    // *(uint32_t *)proc->user_proc.stack_bottom = STACK_BOTTOM_MAGIC_VALUE;

    // // we now need to change the stack ponter 
    // // and to jump to the elf crt0._start() method.
    // // in theory, this will never return, as crt0 will call proc_exit()
    
    // int argc = count_strvec(proc->user_proc.argv);
    // void *stack_top = stack_bottom + stack_size;
    // log_debug("Changing stack pointer to 0x%x and jumping to address 0x%x", stack_top, elf_entry_point);
    // __asm__ __volatile__ (
    //     "mov %0, %%esp\n\t"
    //     "push %3\n\t"
    //     "push %2\n\t"
    //     "push %1\n\t"
    //     "push 0\n\t"  // we are jumping, not calling, so help C detect parameters
    //     "jmp %4\n"
    //     : // no outputs
    //     :
    //         "g"(stack_top), 
    //         "g"(argc),
    //         "g"(proc->user_proc.argv),
    //         "g"(proc->user_proc.envp),
    //         "g"(elf_entry_point)  // jump to the _start() method
    //     : "eax" // mingled registers
    // );

    // log_warn("elf_loader(): somehow, crt0._start() returned, this was not expected!");
    // proc_exit(-5);
}

