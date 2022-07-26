#include <errors.h>
#include <klog.h>
#include <klib/string.h>
#include <filesys/vfs.h>
#include <multitask/process.h>
#include <devices/tty.h>
#include <elf.h>
#include <memory/virtmem.h>
#include <memory/kheap.h>



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
 * Also, exec does not return in case of success. 
 * Maybe we can create a
 */

static void load_and_exec_elf_enrty_point();


int execve(char *path, char *argv[], char *envp[]) {
    klog_trace("execve(path=\"%s\")", path);
    file_t file;
    int err;
    bool file_open = false;

    err = vfs_open(path, &file);
    if (err) goto exit;
    file_open = true;

    err = verify_elf_executable(&file);
    if (err) goto exit;
    
    // gather loading information from the elf file
    void *virt_addr_start = NULL;
    void *virt_addr_end = NULL;
    void *entry_point = NULL;
    err = get_elf_load_information(&file, &virt_addr_start, &virt_addr_end, &entry_point);
    klog_debug("ELF to be loaded at virtual addresses 0x%p - 0x%x, entry point 0x%p", virt_addr_start, virt_addr_end, entry_point);
    if (err) goto exit;

    err = vfs_close(&file);
    if (err) goto exit;
    file_open = false;

    // memory map of a process:
    // 1-4 MB: kernel
    // 128 MB entry point and parts of the elf file (data, rodata, bss, text)
    // below that, stack, growing downwards
    // above that, heap, expandable

    // stack will be located below the executable loaded address (and be 16 bytes aligned)
    int stack_size = 256 * 1024;
    void *stack_bottom = (void *)(((uint32_t)virt_addr_start - stack_size - 4096) & 0xFFFFFFF0);

    // heap will be located above the executable loaded address, growing up
    int heap_size = 0;
    void *heap = (void *)(((uint32_t)virt_addr_end + 0xFFF) & 0xFFFFF000);

    // create something to load the segments (kernel mapped included)
    void *page_directory = create_page_directory(true);
    allocate_virtual_memory_range(stack_bottom, heap + heap_size, page_directory);
    klog_debug("execve() parent proc CR3 is %x, new proc CR3 will be %x", get_page_directory_register(), page_directory);

    // whenever we set CR3, we can load the file into memory.
    // so, we'll set our entry point to a method that, when the task is switched in
    // and CR3 is updated, it will load and jump to the executable.
    // nice answer in Stack Overflow showing the memory space of a process:
    // https://stackoverflow.com/questions/18278803/how-does-elf-file-format-defines-the-stack/18279929#18279929

    
    pid_t ppid = 0;
    uint8_t priority = PRIORITY_USER_PROGRAM;
    tty_t *tty = NULL;
    if (running_process() != NULL) {
        ppid = running_process()->pid;
        priority = running_process()->priority;
        tty = running_process()->tty;
    }

    process_t *proc = create_process(
        load_and_exec_elf_enrty_point,
        path,
        ppid,
        priority,
        stack_bottom + stack_size,
        page_directory,
        tty
    );

    // we need to populate the process with enough data to be able to start.
    proc->heap = heap;
    proc->heap_size = heap_size;
    proc->page_directory = page_directory;  // awfully important! :-) 
    proc->executable_to_load = kmalloc(strlen(path) + 1);
    strcpy(proc->executable_to_load, path);
    proc->argv = argv;
    proc->envp = envp;

    // not much left, cheers!
    klog_debug("execve(): about to start process \"%s\" (pid %d)", proc->name, proc->pid);
    start_process(proc);

    err = SUCCESS;
exit:
    if (file_open)
        vfs_close(&file);
    return err;
}

int exec(char *path) {
    // both argv and envp are considered to be terminated by a null ptr.
    char *null_ptr = NULL;
    return execve(path, &null_ptr, &null_ptr);
}

static void load_and_exec_elf_enrty_point() {
    // supposedly we are a new process and we are to load and execute a new file.
    // all we need will be prepared for us...
    // but how to return some failure indication? maybe the exit() method is the key.
    int err;
    process_t *proc = running_process();

    // grab info from proc, load the executable, jump to start.
    klog_debug("load_and_exec_elf_enrty_point() running, my CR3 is %x", get_page_directory_register());

    // now that we have the correct paging in place, wr can load the file
    file_t file;
    err = vfs_open(proc->executable_to_load, &file);
    if (err) {
        klog_error("Failed opening executable \"%s\"", proc->executable_to_load);
        exit(101);
    }

    void *a, *b, *executable_entry_point;
    err = get_elf_load_information(&file, &a, &b, &executable_entry_point);
    if (err) {
        klog_error("Failed getting executable information \"%s\"", proc->executable_to_load);
        exit(102);
    }

    err = load_elf_into_memory(&file);
    if (err) {
        klog_error("Failed loading executable \"%s\"", proc->executable_to_load);
        exit(103);
    }

    err = vfs_close(&file);
    if (err) {
        klog_error("Failed closing executable \"%s\"", proc->executable_to_load);
        exit(104);
    }

    // jump / call the elf entry point (which is the _start() of crt0)
    ((func_ptr)executable_entry_point)();
}

