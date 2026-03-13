#include "process.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../include/ctypes.h"
#include "../../include/macros.h"
#include "../../logger/logger.h"
#include "../../memory/kheap.h"
#include "../../memory/virtmem.h"
#include "../../klib/string.h"
#include "../../klib/strvec.h"
#include "../elf_reader.h"



MODULE("PROC_LIFE", LOG_LEVEL_WARN);


static pid_t last_pid = 0;
static lock_t pid_lock = 0;






// ----------------------------------------------------------

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = ++last_pid;
    mutex_release(&pid_lock);
    return id;
}

static void proc_add_child(process_t *parent, process_t *child) {
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

static void proc_remove_child(process_t *parent, process_t *child) {
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

// -------------------------------------------------

typedef struct reader_interface {
    error_t (*read)(void *context, size_t offset, void *buffer, size_t size);
    void *context;
} reader_interface_t;

// Placeholder for new memory management functions
error_t proc_memory_allocate_and_load_from_elf(process_t *proc, const char *file_path) {
    log_warn("proc_memory_allocate_and_load_from_elf(proc=%p, file_path=%s) skeleton called", proc, file_path);

    // This function will encapsulate the logic to:
    // 1. Open the ELF file.
    // 2. Parse ELF headers to find loadable segments.
    // 3. For each loadable segment:
    //    a. Allocate physical pages.
    //    b. Map them into the process's page directory with correct permissions.
    //    c. Read data from the ELF file into these mapped pages.
    //    d. Add the mapped region to proc->mmap.
    // 4. Set the process's entry point from the ELF header.
    // 5. Setup the heap segment for the process.
    // 6. Close the ELF file.

    // further idea, facilitates error handling:
    // - for each mem_region needed
    //   - allocate all physical pages
    //   - one-by-one { map, load, unmap }
    //   - map them all to target process

    return ERR_NOT_IMPLEMENTED;
}

error_t proc_memory_allocate_and_copy_from_proc(process_t *dest, process_t *src) {
    log_warn("proc_memory_allocate_and_copy_from_proc(dest=%p, src=%p) skeleton called", dest, src);
    // This function will encapsulate the logic to:
    // 1. Iterate through each memory region in src->mmap.
    // 2. For each region:
    //    a. Allocate new physical pages for the destination process.
    //    b. Map these new pages into dest->page_directory.
    //    c. Copy content from src's region (through its page directory) to dest's new pages.
    //    d. Add the new region to dest->mmap.
    // This includes copying segments for ELF and heap.

    // further idea, facilitates error handling:
    // - for each mem_region needed
    //   - allocate all physical pages
    //   - for each page { map, load, unmap }
    //   - map them all to target process

    return ERR_NOT_IMPLEMENTED;
}

error_t proc_memory_unmap_and_release(process_t *proc) {
    log_warn("proc_memory_unmap_and_release(proc=%p) skeleton called", proc);
    // This function will encapsulate the logic to:
    // 1. Iterate through each memory region in proc->mmap.
    // 2. For each region:
    //    a. Unmap the virtual pages from proc->page_directory.
    //    b. Free the associated physical pages.
    // 3. Clear proc->mmap.

    // further idea:
    // - for each mem_region in the process:
    //   - for each page in region
    //     - { find physical, unmap, release physical }


    return ERR_NOT_IMPLEMENTED;
}

// -------------------

static error_t proc_load_executable_segment(process_t *proc, open_file_t *f, elf_loadable_segment_t *segment) {
    log_info("loading segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c",
        segment->offset_in_file, segment->size_in_file, segment->address_in_mem, segment->size_in_mem, segment->writable ? 'W' : ' ', segment->executable ? 'X' : ' ');

    virt_addr_t mem_start_aligned = vmm_round_down(segment->address_in_mem);
    virt_addr_t mem_end_aligned = vmm_round_up(segment->address_in_mem + segment->size_in_mem);
    size_t num_pages = (mem_end_aligned - mem_start_aligned) / vmm_page_size();

    page_dir_t curr_page_dir = vmm_get_page_directory_register();
    virt_addr_t working_page_address = mem_region_get_mappable_page_address();

    // Permissions for the target mapping
    bool user_accessible = true; // User processes
    bool write_enable = segment->writable; // Writable based on segment flags

    for (size_t page_num = 0; page_num < num_pages; page_num++) {
        // Allocate a physical page
        phys_addr_t page_phys_addr = pmm_allocate_physical_page();
        if (page_phys_addr == 0) {
            log_error("Failed to allocate physical page for ELF segment.");
            // TODO: Clean up
            return ERR_NO_MEMORY;
        }

        // Temporarily map the physical page to a known kernel virtual address
        vmm_map_virtual_to_physical(working_page_address, page_phys_addr, curr_page_dir, true, true);

        // Clear the entire temporary page first
        memset((void *)working_page_address, 0, vmm_page_size());

        // Calculate the virtual address range of the *current page* within the process's address space
        virt_addr_t current_page_virt_start = mem_start_aligned + (page_num * vmm_page_size());
        virt_addr_t current_page_virt_end = current_page_virt_start + vmm_page_size();

        // Determine the actual start and end addresses of the segment's *data* within this current page
        virt_addr_t segment_data_start_in_current_page = max(current_page_virt_start, segment->address_in_mem);
        virt_addr_t segment_data_end_in_current_page   = min(current_page_virt_end, segment->address_in_mem + segment->size_in_file);

        // If there's actual segment data to load into this page
        if (segment_data_start_in_current_page < segment_data_end_in_current_page) {
            size_t bytes_to_read = segment_data_end_in_current_page - segment_data_start_in_current_page;
            size_t dest_buffer_offset = segment_data_start_in_current_page - current_page_virt_start;
            off_t file_read_offset = segment->offset_in_file + (segment_data_start_in_current_page - segment->address_in_mem);

            error_t seek_err = vfs_seek(f, file_read_offset, SEEK_SET);
            if (seek_err) {
                log_error("ELF loader: Failed to seek in file (0x%x): %d", file_read_offset, seek_err);
                vmm_unmap(working_page_address, curr_page_dir);
                pmm_free_physical_page(page_phys_addr);
                return seek_err;
            }

            ssize_t bytes_read = vfs_read(f, (void *)(working_page_address + dest_buffer_offset), bytes_to_read);
            if (bytes_read < 0) {
                log_error("ELF loader: Failed to read from file: %d", (error_t)bytes_read);
                vmm_unmap(working_page_address, curr_page_dir);
                pmm_free_physical_page(page_phys_addr);
                return (error_t)bytes_read;
            }
            if ((size_t)bytes_read != bytes_to_read) {
                log_warn("ELF loader: Short read for segment (0x%x). Expected %u, got %zd.",
                         segment->address_in_mem, bytes_to_read, bytes_read);
                vmm_unmap(working_page_address, curr_page_dir);
                pmm_free_physical_page(page_phys_addr);
                return ERR_READING_FILE;
            }
        }

        // Unmap the temporary page
        vmm_unmap(working_page_address, curr_page_dir);

        // Map the physical page to its final virtual address in the process's page directory
        vmm_map_virtual_to_physical(current_page_virt_start, page_phys_addr, proc->page_directory, user_accessible, write_enable);
    }

    return OK;
}

static error_t proc_load_executable_segments(process_t *proc, const char *file_path) {
    open_file_t *f;

    error_t err = vfs_open(file_path, 0, &f);
    if (err) return err;

    err = elf_verify_executable(f);
    if (err) { vfs_close(f); return err; }

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

    err = elf_get_entry_point(f, (virt_addr_t *)&proc->entry_point);
    if (err) return err;
    proc->name = kstrdup(file_path);

    return OK;
}

static process_t *_create_process_base(page_dir_t pd, uintptr_t stack_top, size_t stack_size, process_t *parent, proc_priority_t priority, const char *name)  {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    memset(proc, 0, sizeof(process_t));
    
    proc->pid = next_pid();
    if (parent != NULL) {
        proc->parent = parent;
        proc_add_child(parent, proc);
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

process_t *create_user_process(process_t *parent, const char *file_path) {
    // for user processes, stack is at 1GB growing downwards
    size_t stack_size = 64 * KB;
    uintptr_t stack_top = (1 * GB) - stack_size;
    page_dir_t pd = vmm_create_page_directory(true);

    process_t *p = _create_process_base(pd, stack_top, stack_size, parent, PRIORITY_USER_PROGRAM, file_path);
    if (p == NULL) {
        vmm_destroy_page_directory(pd); // Clean up page directory if process creation fails
        return NULL;
    }
    
    // Assign the new page directory to the process
    p->page_directory = pd;

    // Load executable segments
    error_t err = proc_load_executable_segments(p, file_path);
    if (err) {
        log_error("Failed to load executable segments for %s: %d", file_path, err);
        // TODO: Proper cleanup, including unmapping regions that were already mapped
        // For now, destroy the page directory and free the process struct
        vmm_destroy_page_directory(pd);
        kfree(p);
        return NULL;
    }

    // Map user stack region
    // User stack needs to be user accessible and writable
    mem_region_t user_stack_region = mem_region_of(stack_top, stack_size, REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_STACK, "user_stack");
    err = mem_region_allocate_clear_and_map(&user_stack_region, p->page_directory);
    if (err) {
        log_error("Failed to allocate and map user stack for %s: %d", file_path, err);
        // TODO: Proper cleanup
        vmm_destroy_page_directory(pd);
        kfree(p);
        return NULL;
    }
    // Set the process's stack pointer to the top of the user stack
    // The stack grows downwards, so esp should point to the highest address initially.
    p->esp = stack_top + stack_size; 

    // The entry point is already set by proc_load_executable_segments

    log_trace("create_user_process(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}

// ------------------------------------------------
// original proc create code below
// ------------------------------------------------

static void _unlock_and_run_entry_point() {
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
    p->stack_snapshot->return_address = (uint32_t)_unlock_and_run_entry_point;
    p->page_directory = vmm_get_kernel_page_directory();  

    // what our _unlock_and_run_entry_point() should call
    p->entry_point = entry_point;

    // set working directory
    proc_chdir(p, "/");

    log_trace("process_create(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
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

