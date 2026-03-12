#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../include/ctypes.h"
#include "../../logger/logger.h"
#include "../../memory/kheap.h"
#include "../../memory/virtmem.h"
#include "../../klib/string.h"
#include "../../klib/strvec.h"
#include "elf_reader.h"



MODULE("PROC_LIFE", LOG_LEVEL_WARN);


static pid_t last_pid = 0;
static lock_t pid_lock = 0;




// starts a process, by putting it on the ready list.
void proc_start(process_t *process) {

    // if we have not started multitasking yet... not much
    if (!multitasking_enabled()) {
        proclist_append(&ready_lists[process->priority], process);
        return;
    }

    lock_scheduler();
    proclist_append(&ready_lists[process->priority], process);

    // if running task is lower priority (e.g. idle task), preempt it
    if (running_process() != NULL && process->priority < running_process()->priority)
        schedule();
    
    unlock_scheduler();
}


static void proc_cleanup() {
    // unlock the scheduler in our first execution
    unlock_scheduler(); 

    // we can now call the entry point.
    // for kernel tasks, this is a method in kernel space.
    // for exec(), this is a kernel method to load and run the executable
    // the called method should not return, but call exit() to exit.
    process_t *r = running_process();
    r->entry_point();

    // terminate and later free the process
    log_warn("process(): It seems main returned");
    proc_exit(r, -7);
}


// create but don't start yet
process_t *create_process(bool is_kernel, char *name, func_ptr entry_point, proc_priority_t priority, process_t *parent, tty_t *tty) {
    if (priority >= PROCESS_PRIORITY_LEVELS) {
        log_warn("priority %d requested when we only have %d levels", priority, PROCESS_PRIORITY_LEVELS);
        return NULL;
    }

    /*
        it seems this is not so easy. So processes will belong to kernel, e.g. the idle task,
        so they only need stack, 
        and some processes will belong to user programs, so we don't know yet what kind of memory 
        mapping they will need.
        sometimes we'll toss it out to reset (exec), sometimes copy (fork) and sometimes set (kernel)
    */

    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    memset(p, 0, sizeof(process_t));
    
    mutex_acquire(&pid_lock);
    p->pid = ++last_pid;
    mutex_release(&pid_lock);

    p->parent = parent;
    p->priority = priority;
    p->tty = tty;
    p->name = kmalloc(strlen(name) + 1);
    strcpy(p->name, name);
    p->state = READY;

    // every process gets this small stack, to be able to switch in
    // since this is inside kernel's mapped memory, no paging faults should occur
    int stack_size = 4096;
    p->allocated_kernel_stack = kmalloc(stack_size);
    memset(p->allocated_kernel_stack, 0, stack_size);
    *(uint32_t *)p->allocated_kernel_stack = STACK_BOTTOM_MAGIC_VALUE;

    // we now have a small stack to set the "return" address for the first switching.
    // how can we setup the initial stack, i.e. the arguments to that entry point?
    p->esp = (uint32_t)(p->allocated_kernel_stack + stack_size - sizeof(switched_stack_snapshot_t));
    p->stack_snapshot->return_address = (uint32_t)proc_cleanup;
    p->page_directory = vmm_get_kernel_page_directory();  

    // what our proc_cleanup() should call
    p->entry_point = entry_point;

    // set working directory
    proc_chdir(p, "/");

    log_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}

// ----------------------------------------------------------

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = ++last_pid;
    mutex_release(&pid_lock);
    return id;
}

static void add_proc_child(process_t *parent, process_t *child) {
    if (parent == NULL || child == NULL)
        return;

    if (parent->children_list == NULL) {
        parent->children_list = child;
    } else {
        process_t *p = parent->children_list;
        while (p->next_child != NULL)
            p = p->next_child;
        p->next_child = child;
    }
    child->next_child = NULL;
}

static void remove_proc_child(process_t *parent, process_t *child) {
    if (parent == NULL || child == NULL)
        return;

    if (parent->children_list == child) {
        parent->children_list = child->next_child;
    } else {
        process_t *p = parent->children_list;
        while (p->next_child != NULL && p->next_child != child)
            p = p->next_child;
        if (p->next_child != NULL)
            p->next_child = p->next_child->next_child;
    }
    child->next_child = NULL;
}

// TODO: maybe this scheme will work
// - but first make process work with only mem_map()
// create_base_process_without_memory()
// update_process_release_memory()
// update_process_add_memory_for_executable()
// update_process_add_memory_for_kernel()

static process_t *_create_process_base(page_dir_t pd, uintptr_t stack_top, size_t stack_size, process_t *parent, proc_priority_t priority, const char *name)  {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    memset(proc, 0, sizeof(process_t));
    
    proc->pid = next_pid();
    if (parent != NULL) {
        proc->parent = parent;
        add_proc_child(parent, proc);
    }
    proc->priority = priority;
    proc->name = kstrdup(name);

    // set the cwd
    // set the open files (stdin, stdout, stderr)
    // push env and argv
    return proc;
}

process_t *create_kernel_process(const char *name, uintptr_t function_to_call) {
    static int kernel_processes_count = 0; 

    // since kernel is up to 64MB, let's say task stacks will be 64MB downwards
    uintptr_t stacks_ceiling = 64 * MB;
    size_t stack_size = 8 * KB;
    uintptr_t stack_top = stacks_ceiling - (stack_size * kernel_processes_count);
    kernel_processes_count += 1;

    process_t *p = _create_process_base(vmm_get_kernel_page_directory(), stack_top, stack_size, NULL, PRIORITY_KERNEL_TASK, name);

    // there's little more to do here, isn't it
    // set the entry point.

    return p;
}


typedef struct _elf_loading_filler_struct {
    open_file_t *file;
    virt_addr_t mem_start;
    elf_loadable_segment_t *segment;
} _elf_loading_filler_struct_t;

static error_t _elf_loading_filler_func(size_t page_num, uintptr_t dest_addr, void *context) {
    _elf_loading_filler_struct_t *ctx = (_elf_loading_filler_struct_t *)context;
    elf_loadable_segment_t *segment = ctx->segment;
    open_file_t *file = ctx->file;

    // 1. Clear the entire temporary page
    memset((void *)dest_addr, 0, vmm_page_size());

    // Calculate the virtual address range of the *current page* within the process's address space
    virt_addr_t segment_region_start_aligned = vmm_round_down(segment->address_in_mem);
    virt_addr_t current_page_virt_start = segment_region_start_aligned + (page_num * vmm_page_size());
    virt_addr_t current_page_virt_end = current_page_virt_start + vmm_page_size();

    // Determine the actual start and end addresses of the segment's *data* within this current page
    // This handles cases where the segment starts or ends mid-page.
    virt_addr_t segment_data_start_in_current_page = max(current_page_virt_start, segment->address_in_mem);
    virt_addr_t segment_data_end_in_current_page   = min(current_page_virt_end, segment->address_in_mem + segment->size_in_file);

    // If there's no actual segment data in this page, we're done (it's already zeroed)
    if (segment_data_start_in_current_page >= segment_data_end_in_current_page) {
        return OK;
    }

    // Calculate how many bytes to read for this page from the file
    size_t bytes_to_read = segment_data_end_in_current_page - segment_data_start_in_current_page;

    // Calculate the offset within the temporary kernel page (`dest_addr`) where the data should be written
    size_t dest_buffer_offset = segment_data_start_in_current_page - current_page_virt_start;

    // Calculate the offset in the ELF file to read from
    // This is the segment's file offset plus the offset of segment data within the *total* memory region
    off_t file_read_offset = segment->offset_in_file + (segment_data_start_in_current_page - segment->address_in_mem);

    // Perform the read operation
    error_t err = vfs_seek(file, file_read_offset, SEEK_SET);
    if (err) {
        log_error("ELF filler: Failed to seek in file (0x%x): %d", file_read_offset, err);
        return err;
    }

    ssize_t bytes_read = vfs_read(file, (void *)(dest_addr + dest_buffer_offset), bytes_to_read);
    if (bytes_read < 0) { // vfs_read returns negative error code on failure
        log_error("ELF filler: Failed to read from file: %d", (error_t)bytes_read);
        return (error_t)bytes_read;
    }
    if ((size_t)bytes_read != bytes_to_read) {
        log_warn("ELF filler: Short read for segment (0x%x). Expected %u, got %d.",
                 segment->address_in_mem, bytes_to_read, bytes_read);
        return ERR_READING_FILE; // Or handle as a warning if partial reads are acceptable in some cases.
    }

    return OK;
}

static error_t proc_load_executable_segment(process_t *proc, open_file_t *f, elf_loadable_segment_t *segment) {
    log_info("loading segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c", 
        segment->offset_in_file, segment->size_in_file, segment->address_in_mem, segment->size_in_mem, segment->writable ? 'W' : ' ', segment->executable ? 'X' : ' ');

    virt_addr_t mem_start = vmm_round_down(segment->address_in_mem);
    virt_addr_t mem_stop = vmm_round_up(segment->address_in_mem + segment->size_in_mem);
    size_t mem_size = mem_stop - mem_start;

    _elf_loading_filler_struct_t filler_ctx = {
        .file = f,
        .segment = segment,
        .mem_start = mem_start,
    };
    page_filler_t filler = {
        .fill_page = _elf_loading_filler_func,
        .context = &filler_ctx
    };
    
    mem_region_t region = mem_region_of(mem_start, mem_size, segment->writable ? REGION_WRITE_ENABLE : REGION_READ_ONLY);
    error_t err = mem_region_allocate_fill_and_map(&region, proc->page_directory, &filler);

    return err;
}

static error_t proc_load_executable_segments(process_t *proc, const char *file_path) {
    error_t err = elf_verify_executable(file_path);
    if (err) return err;

    open_file_t *f;
    err = vfs_open(file_path, 0, &f);
    if (err) return err;

    int headers = 0;
    err = elf_get_program_headers_count(f, &headers);
    if (err) return err;

    elf_loadable_segment_t *segments_arr = kmalloc(sizeof(elf_loadable_segment_t) * headers);
    if (segments_arr == NULL) return ERR_NO_MEMORY;

    err = elf_get_program_headers_info(f, segments_arr, headers);
    if (err) { kfree(segments_arr); return err; }

    // now load the headers
    for (int i = 0; i < headers; i++) {
        err = proc_load_executable_segment(proc, f, &segments_arr[i]);
        if (err) { kfree(segments_arr); return err; }
    }

    kfree(segments_arr);

    err = elf_get_entry_point(f, (virt_addr_t **)&proc->entry_point);
    if (err) return err;
    proc->name = kstrdup(file_path);

    return OK;
}

process_t *create_user_process(process_t *parent, const char *file_path) {
    // for user processes, stack is at 1GB growing downwards
    size_t stack_size = 64 * KB;
    uintptr_t stack_top = (1 * GB) - stack_size;
    page_dir_t pd = vmm_create_page_directory(true);

    process_t *p = _create_process_base(pd, stack_top, stack_size, parent, PRIORITY_USER_PROGRAM, file_path);

    // TODO: continue here. proc will bring together mem_region, elf and vmm.
    // TODO: rename and convert elf_loader into elf_reader. 
    if (!proc_load_executable_segments(p, file_path))
        // ... ?


    // we could / should open the file and load the segments here...
    // then setup the entry point
    // find how many loadable segments in the ELF file
    // foreach segment:
    //   - read size and flags
    //   - prepare memory region
    //   - for each page of that segment:
    //     - allocate physical page
    //     - map the page in known address of the current CR3
    //     - load from file onto page
    //     - unmap page and map at target of the target CR3

    // then query ELF to find entry point


    return p;
}

error_t proc_add_memory(process_t *proc) {
    // don't touch stack, add text/data
    return ERR_NOT_IMPLEMENTED;
}

error_t proc_release_memory(process_t *proc) {
    // don't touch stack, just text/data
    for (int i = 0; i < proc->mmap.count; i++) {
        mem_region_unmap_and_release(&proc->mmap.regions[i], proc->page_directory);
    }
    return ERR_NOT_IMPLEMENTED;
}

error_t proc_copy_memory(process_t *dest, process_t *src) {
    // don't touch stack, copy text/data
    for (int i = 0; i < src->mmap.count; i++) {
        mem_region_t *sr = &src->mmap.regions[i];
        mem_region_t dr = mem_region_of(sr->address, sr->size, sr->flags, sr->name);
        mem_region_allocate_copy_and_map(&dr, dest->page_directory, sr->address); // warning, uses current CR3
        mem_map_add_region(&dest->mmap, dr);
    }
    return ERR_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------

// a task can ask to be terminated
void proc_exit(process_t *proc, int exit_code) {
    lock_scheduler();

    proc->state = TERMINATED;
    proc->exit_code = exit_code;
    proclist_append(&terminated_list, proc);
    log_trace("Process %s[%d] exited, exit code %d", proc->name, proc->pid, exit_code);

    // possibly wake up parent process
    process_t *parent = proc->parent;
    if (parent != NULL && parent->state == BLOCKED && parent->block_reason == WAIT_CHILD_EXIT) {
        log_trace("Will unblock parent process %s[%d]", parent->name, parent->pid);
        parent->terminated_child_pid = proc->pid;
        parent->terminated_child_exit_code = exit_code;
        log_debug("Added pid %d and exit code %d to parent", proc->pid, exit_code);
        proc_unblock(parent);
    }

    // whether we unblocked parent or not, somebody else should run
    schedule();
    unlock_scheduler();
}


// after a process has terminated, clean up resources
void proc_destroy(process_t *proc) {
    // be careful with the exec() process, it may have allocated more resources
    if (proc->name != NULL)
        kfree(proc->name);

    if (proc->allocated_kernel_stack != 0)
        kfree(proc->allocated_kernel_stack);

    if (proc->page_directory != 0 && proc->page_directory != vmm_get_kernel_page_directory())
        vmm_destroy_page_directory(proc->page_directory);

    if (proc->user_proc.executable_path != NULL)
        kfree(proc->user_proc.executable_path);
    if (proc->user_proc.argv != NULL)
        free_strvec(proc->user_proc.argv);
    if (proc->user_proc.envp != NULL)
        free_strvec(proc->user_proc.envp);
    
    if (proc->curr_dir_path != NULL)
        kfree(proc->curr_dir_path);
    
    // can't think of anything else to free
    kfree(proc);
}

