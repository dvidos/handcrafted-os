#include <errors.h>
#include <klog.h>
#include <klib/string.h>
#include <filesys/vfs.h>
#include <multitask/process.h>


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
 * Also notice that exec() does not allocate a new process, this is done via fork() only.
 * I do not remember why this is, but I read the reason in one of the pdfs.
 * 
 * Also, exec does not return in case of success. 
 * Maybe we can create a
 */


static int load_executable_file(char *path) {
    file_t file_data;
    file_t *file;
    char buffer[64];

    file = &file_data;

    // let's deal with "#!/bin/sh" constructs later.
    // let's support ELF header for now
    
    int err = vfs_open(path, file);
    if (err)
        return err;

    err = vfs_read(file, buffer, 4);
    if (err != 4) {
        vfs_close(file);
        return ERR_NOT_SUPPORTED;
    }

    if (buffer[0] == '#' && buffer[1] == '!') {
        klog_warn("Interpreter mark detected, but not yet supported");
        vfs_close(file);
        return ERR_NOT_SUPPORTED;
    }

    if (buffer[0] != 0x7F || memcmp(buffer + 1, "ELF", 3) != 0) {
        vfs_close(file);
        return ERR_NOT_SUPPORTED;
    }

    klog_debug("ELF header detected, proceeding");
    
    // in theory, in successful execution, this replaces all of this process?
    // but this process is not user space !?!?!?

    // we should grab pages, maybe map virtual to physical etc.
    extern int load_elf_file(file_t *file);
    err = load_elf_file(file);
    klog_debug("load_elf_file() -> %d", err);
    vfs_close(file);
    
    return err;
}

int execve(char *path, char *argv[], char *envp[]) {
    klog_trace("execve(path=\"%s\")", path);
    int err = load_executable_file(path);
    klog_debug("load_executable_file() -> %d", err);

    // then maybe we create a process etc.
    // maybe map some virtual memory etc.
    // sounds like we cannot execute a file outside of the multitasking environment...

    return err;
}

char *null_ptr = NULL;

int exec(char *path) {
    // both argv and envp are considered to be terminated by a null ptr.
    return execve(path, &null_ptr, &null_ptr);
}
