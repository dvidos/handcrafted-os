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
#include "../../utils/assert.h"
#include "../elf_reader.h"



MODULE("PROC_CREATE", LOG_LEVEL_WARN);



// ----------------------------------------------------------

static pid_t last_pid = 0;
static lock_t pid_lock = 0;

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = ++last_pid;
    mutex_release(&pid_lock);
    return id;
}

// ----------------------------------------------------------

static void proc_add_child(process_t *parent, process_t *child) {
    ASSERT(parent != NULL);
    ASSERT(child != NULL);

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
    ASSERT(parent != NULL);
    ASSERT(child != NULL);

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

// ----------------------------------------------------------------

static error_t _allocate_lot_of_physical_pages_atomically(int num_pages, phys_addr_t **addresses_arr) {
    ASSERT(num_pages > 0);
    ASSERT(addresses_arr != NULL);

    phys_addr_t *arr = kmalloc(sizeof(phys_addr_t) * num_pages);
    if (arr == NULL) return ERR_NO_MEMORY;
    memset(arr, 0, sizeof(phys_addr_t) * num_pages);

    bool failed = false;
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t addr = pmm_allocate_physical_page();
        if (addr == 0) { failed = true; break; }
        arr[page] = addr;
    }

    if (failed) {
        for (int page = 0; page < num_pages; page++)
            if (arr[page])
                pmm_free_physical_page(arr[page]);
        return ERR_NO_MEMORY;
    }

    *addresses_arr = arr;
    return OK;
}

static error_t _release_lot_of_physical_pages_atomically(phys_addr_t *addresses_arr, int num_pages) {
    ASSERT(addresses_arr != NULL);
    ASSERT(num_pages > 0);

    for (int page = 0; page < num_pages; page++) {
        if (addresses_arr[page])
            pmm_free_physical_page(addresses_arr[page]);
    }

    kfree(addresses_arr);
    return OK;
}

static error_t _region_allocate_and_map(mem_region_t *reg, page_dir_t page_dir) {
    ASSERT(reg != NULL);
    ASSERT(page_dir > 0);

    phys_addr_t *pages_arr;
    int num_pages = vmm_pages_for_size(reg->size);
    error_t err = _allocate_lot_of_physical_pages_atomically(num_pages, &pages_arr);
    if (err) return err;

    virt_addr_t vaddr = reg->address;
    bool user = reg->flags & REGION_USER_ACCESSIBLE;
    bool writable = reg->flags & REGION_WRITE_ENABLE;
    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_virtual_to_physical(vaddr, pages_arr[page], page_dir, user, writable);
        // TODO: fix error recovery
        vaddr += vmm_page_size();
    }
    
    kfree(pages_arr);
    return OK;
}

static error_t _region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir) {
    ASSERT(reg != NULL);
    ASSERT(page_dir > 0);
    ASSERT(vmm_is_page_aligned(reg->address));
    ASSERT(vmm_is_page_aligned(reg->size));

    int num_pages = reg->size / vmm_page_size();
    virt_addr_t vaddr = reg->address;
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t paddr = vmm_resolve(vaddr, page_dir);
        vmm_unmap(vaddr, page_dir);
        pmm_free_physical_page(paddr);
        vaddr += vmm_page_size();
    }

    *reg = mem_region_empty();
    return OK;
}

static error_t proc_allocate_and_map_stack(process_t *proc, size_t size, virt_addr_t stack_top) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(stack_top > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(stack_top));

    region_flags_t flags = REGION_USAGE_STACK | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(stack_top - size, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.stack = reg;
    return OK;
}

static error_t proc_allocate_and_map_heap(process_t *proc, size_t size, virt_addr_t heap_base) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(heap_base > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(heap_base));

    region_flags_t flags = REGION_USAGE_HEAP | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(heap_base, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.heap = reg;
    return OK;
}

static error_t proc_allocate_and_map_elf_segment(process_t *proc, elf_loadable_segment_t *seg) {
    ASSERT(proc != NULL);
    ASSERT(seg != NULL);
    ASSERT(proc->memory.elf_sections_count < MAX_PROCESS_ELF_SECTIONS);

    virt_addr_t addr = vmm_round_down(seg->address_in_mem);
    size_t size = vmm_round_up(seg->address_in_mem + seg->size_in_mem) - addr;
    region_flags_t flags = REGION_USAGE_ELF |
        (seg->writable ? REGION_WRITE_ENABLE : REGION_READ_ONLY) |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    
    mem_region_t reg = mem_region_of(addr, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.elf_sections[proc->memory.elf_sections_count] = reg;
    proc->memory.elf_sections_count += 1;
    return OK;
}

// ##############################################################################

typedef struct reader_interface {
    error_t (*read)(void *context, size_t offset, void *buffer, size_t size);
    void *context;
} reader_interface_t;

static error_t _memory_allocate_and_map_stack_region(int kilobytes, virt_addr_t stack_top, process_t *proc) {
    phys_addr_t *pages_arr;
    int num_pages = vmm_round_up(kilobytes * 1024) / vmm_page_size();
    error_t err = _allocate_lot_of_physical_pages_atomically(num_pages, &pages_arr);
    if (err) return err;

    virt_addr_t stack_base = stack_top - (num_pages * vmm_page_size());
    bool user_accessible = proc_is_user_proc(proc);

    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_virtual_to_physical(stack_base + page * vmm_page_size(), pages_arr[page], proc->memory.page_dir, user_accessible, true);
        // TODO: better error handling
    }
    
    proc->memory.stack = mem_region_of(stack_base, num_pages * vmm_page_size(),
        REGION_USAGE_STACK | (user_accessible ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY));
    kfree(pages_arr);

    return OK;
}

static error_t _memory_allocate_and_map_heap_region(int kilobytes, virt_addr_t heap_addr, process_t *proc) {
    phys_addr_t *pages_arr;
    int num_pages = vmm_round_up(kilobytes * 1024) / vmm_page_size();
    error_t err = _allocate_lot_of_physical_pages_atomically(num_pages, &pages_arr);
    if (err) return err;

    bool user_accessible = proc_is_user_proc(proc);
    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_virtual_to_physical(heap_addr + page * vmm_page_size(), pages_arr[page], proc->memory.page_dir, user_accessible, true);
        // TODO: better error handling
    }
    
    proc->memory.stack = mem_region_of(heap_addr, num_pages * vmm_page_size(),
        REGION_USAGE_HEAP | (user_accessible ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY) | REGION_WRITE_ENABLE);
    kfree(pages_arr);

    return OK;
}

static error_t proc_memory_read_elf_chunk_into_memory(open_file_t *elf, off_t offset, size_t size, virt_addr_t target) {
    if (size == 0) return OK;

    off_t new_offset = vfs_seek(elf, offset, SEEK_SET);
    if (new_offset != offset) return ERR_READING_FILE;

    ssize_t bytes = vfs_read(elf, (void *)target, size);
    if (bytes < 0) return bytes;
    if ((size_t)bytes != size) return ERR_READING_FILE;

    return OK;
}

static void proc_memory_calculate_elf_segment_metrics(elf_loadable_segment_t *seg, int page_no, off_t *file_offset, size_t *page_offset, size_t *chunk_len) {
    // file offsets do not always align with 4k in memory.

    virt_addr_t page_start = vmm_round_down(seg->address_in_mem);
    size_t first_page_gap = seg->address_in_mem - page_start; // so if he wants to load at 5000, the gap is (5000-4096)=904

    // example, if he wants us to load 5k of data, at address 5k, we need to
    // - load 3k towards the end of the first page
    // - load 2k towards the start of the second page


    //     // Calculate the virtual address of the current page within the process's address space.                                                                                              │
    //     // The segment's address_in_mem might not be page-aligned, so we find the first page                                                                                                  │
    //     // that contains part of this segment.                                                                                                                                                │
    //     virt_addr_t segment_first_page_vaddr = vmm_round_down(seg->address_in_mem);                                                                                                           │
    //     virt_addr_t current_page_vaddr = segment_first_page_vaddr + (page_no * vmm_page_size());                                                                                              │
    //                                                                                                                                                                                           │
    //     // Determine the actual start and end addresses of the segment's *data* within this current page                                                                                      │
    //     virt_addr_t segment_data_start_in_current_page = max(current_page_vaddr, seg->address_in_mem);                                                                                        │
    //     virt_addr_t segment_data_end_in_current_page = min(current_page_vaddr + vmm_page_size(), seg->address_in_mem + seg->size_in_file);                                                    │
    //                                                                                                                                                                                           │
    //     // If there's actual segment data to load into this page                                                                                                                              │
    //     if (segment_data_start_in_current_page < segment_data_end_in_current_page) {                                                                                                          │
    //         *copy_size = segment_data_end_in_current_page - segment_data_start_in_current_page;                                                                                               │
    //         *mem_page_offset = segment_data_start_in_current_page - current_page_vaddr;                                                                                                       │
    //         *file_offset = seg->offset_in_file + (segment_data_start_in_current_page - seg->address_in_mem);                                                                                  │
    //     } else {                                                                                                                                                                              │
    //         // No file data for this portion of the segment in this page                                                                                                                      │
    //         *copy_size = 0;                                                                                                                                                                   │
    //         *mem_page_offset = 0;                                                                                                                                                             │
    //         *file_offset = 0;                                                                                                                                                                 │
    //     }                                                                                                                                                                                     │
    // }                                    
}

static error_t _memory_allocate_and_load_from_elf_one_segment(process_t *proc, open_file_t *elf, int header_no, elf_loadable_segment_t *seg) {
    error_t err = OK;
    phys_addr_t *phys_addresses = NULL;

    // this is wrong, it may take 2 pages but be less than 4096...
    size_t total_size = round_up(seg->size_in_mem, vmm_page_size());
    int num_pages = total_size / vmm_page_size();
    phys_addresses = kmalloc(sizeof(phys_addr_t) * num_pages);
    if (phys_addresses == NULL) { err = ERR_NO_MEMORY; goto exit; }

    // allocate all, to enable clean release
    memset(phys_addresses, 0, sizeof(phys_addr_t) * num_pages);
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t addr = pmm_allocate_physical_page();
        if (addr == 0) { err = ERR_NO_MEMORY; goto exit; }
        phys_addresses[page] = addr;
    }

    page_dir_t curr_page_dir = vmm_get_page_directory_register();
    virt_addr_t copy_area_addr = vmm_get_kernel_copy_area_address();
    virt_addr_t base_vaddr = vmm_round_down(seg->address_in_mem);

    // for each: { map, load, unmap, map on target page_dir }
    for (int page = 0; page < num_pages; page++) {
        // temp mapping to copy
        phys_addr_t page_addr = phys_addresses[page];
        err = vmm_map_virtual_to_physical(copy_area_addr, page_addr, curr_page_dir, false, true);
        // TODO: better error handling
        memset((void *)copy_area_addr, 0, vmm_page_size());

        // load from file into page, pay attention to offsets as things don't always align
        off_t file_offset = 0;
        size_t page_offset = 0;
        size_t chunk_len = 0;
        proc_memory_calculate_elf_segment_metrics(seg, page, &file_offset, &page_offset, &chunk_len);
        err = proc_memory_read_elf_chunk_into_memory(elf, file_offset, chunk_len, copy_area_addr + page_offset);
        if (err) { vmm_unmap(copy_area_addr, curr_page_dir); err = ERR_READING_FILE; goto exit; }

        // move to final mapping, using proc's properties
        vmm_unmap(copy_area_addr, curr_page_dir);
        err = vmm_map_virtual_to_physical(base_vaddr + page * vmm_page_size(), page_addr, proc->memory.page_dir, true, seg->writable);
        // TODO: better error handling
    }

    // we've loaded and prepared all the loadable pages to the target proc!
    // should fill in the proc->memory.elf_sections[h] section.
    proc->memory.elf_sections[proc->memory.elf_sections_count].address = base_vaddr;
    proc->memory.elf_sections[proc->memory.elf_sections_count].size = num_pages * vmm_page_size();
    proc->memory.elf_sections[proc->memory.elf_sections_count].flags = seg->writable ? REGION_WRITE_ENABLE : 0;

exit:
    if (phys_addresses != NULL) {
        for (int i = 0; i < num_pages; i++)
            if (phys_addresses[i])
                pmm_free_physical_page(phys_addresses[i]);
        kfree(phys_addresses);
    }
    // free all physical pages
    return err;
}

static error_t _memory_allocate_and_load_from_elf_all_segments(process_t *proc, const char *file_path) {
    log_trace("_memory_allocate_and_load_from_elf_all_segments(proc=%p, file_path=%s)", proc, file_path);

    open_file_t *f = NULL;
    error_t err = OK;
    elf_loadable_segment_t *segments_arr = NULL;
    
    err = vfs_open(file_path, 0, &f);
    if (err) goto exit;

    err = elf_verify_executable(f);
    if (err) { vfs_close(f); return err; }

    int headers = 0;
    err = elf_get_program_headers_count(f, &headers);
    if (err) goto exit;

    segments_arr = kmalloc(sizeof(elf_loadable_segment_t) * headers);
    if (segments_arr == NULL) { err = ERR_NO_MEMORY; goto exit; }

    err = elf_get_program_headers_info(f, segments_arr, headers);
    if (err) goto exit;

    for (int h = 0; h < headers; h++) {
        err = _memory_allocate_and_load_from_elf_one_segment(proc, f, h, &segments_arr[h]);
        if (err) goto exit;
    }

    // TODO: stack, heap?

exit:
    if (segments_arr) kfree(segments_arr);
    if (f) vfs_close(f);
    return err;
}

static error_t _memory_allocate_and_copy_from_proc(process_t *dest, process_t *src) {
    log_warn("_memory_allocate_and_copy_from_proc(dest=%p, src=%p) skeleton called", dest, src);
    // This function will encapsulate the logic to:
    // 1. Iterate through each memory region in src->mmap.
    // 2. For each region:
    //    a. Allocate new physical pages for the destination process.
    //    b. Map these new pages into dest->memory.page_dir.
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

static error_t _memory_unmap_and_release(process_t *proc) {
    log_warn("proc_memory_unmap_and_release(proc=%p) skeleton called", proc);
    // This function will encapsulate the logic to:
    // 1. Iterate through each memory region in proc->mmap.
    // 2. For each region:
    //    a. Unmap the virtual pages from proc->memory.page_dir.
    //    b. Free the associated physical pages.
    // 3. Clear proc->mmap.

    // further idea:
    // - for each mem_region in the process:
    //   - for each page in region
    //     - { find physical, unmap, release physical }


    return ERR_NOT_IMPLEMENTED;
}

// ##############################################################################

static error_t proc_load_executable_segment(process_t *proc, open_file_t *f, elf_loadable_segment_t *segment) {
    log_info("loading segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c",
        segment->offset_in_file, segment->size_in_file, segment->address_in_mem, segment->size_in_mem, segment->writable ? 'W' : ' ', segment->executable ? 'X' : ' ');

    error_t err;
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
        err = vmm_map_virtual_to_physical(working_page_address, page_phys_addr, curr_page_dir, true, true);
        // TODO: better error handling

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
        err = vmm_map_virtual_to_physical(current_page_virt_start, page_phys_addr, proc->memory.page_dir, user_accessible, write_enable);
        // TODO: better error handling
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
    if (proc == NULL) return NULL;

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
    p->memory.page_dir = pd;

    // Load executable segments
    // error_t err = proc_load_executable_segments(p, file_path);
    // if (err) {
    //     log_error("Failed to load executable segments for %s: %d", file_path, err);
    //     // TODO: Proper cleanup, including unmapping regions that were already mapped
    //     // For now, destroy the page directory and free the process struct
    //     vmm_destroy_page_directory(pd);
    //     kfree(p);
    //     return NULL;
    // }

    // // Map user stack region
    // // User stack needs to be user accessible and writable
    // mem_region_t user_stack_region = mem_region_of(stack_top, stack_size, REGION_USER_ACCESSIBLE | REGION_WRITE_ENABLE | REGION_USAGE_STACK, "user_stack");
    // err = mem_region_allocate_clear_and_map(&user_stack_region, p->memory.page_dir);
    // if (err) {
    //     log_error("Failed to allocate and map user stack for %s: %d", file_path, err);
    //     // TODO: Proper cleanup
    //     vmm_destroy_page_directory(pd);
    //     kfree(p);
    //     return NULL;
    // }
    // // Set the process's stack pointer to the top of the user stack
    // // The stack grows downwards, so esp should point to the highest address initially.
    // p->esp = stack_top + stack_size; 

    // // The entry point is already set by proc_load_executable_segments

    log_trace("create_user_process(name=\"%s\") -> PID %d, ptr 0x%p", p->name, p->pid, p);
    return p;
}

// --------------- original kernel's attempt ---------------------------------
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
    ((func_ptr)r->entry_point)();

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
    p->memory.page_dir = vmm_get_kernel_page_directory();  

    // what our _unlock_and_run_entry_point() should call
    p->entry_point = (uintptr_t)entry_point;

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

    if (proc->memory.page_dir != 0 && proc->memory.page_dir != vmm_get_kernel_page_directory())
        vmm_destroy_page_directory(proc->memory.page_dir);

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
