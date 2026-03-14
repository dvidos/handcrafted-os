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

static pid_t   next_pid();
static int     next_kernel_stack_no();
static void    proc_add_child(process_t *parent, process_t *child);
static void    proc_remove_child(process_t *parent, process_t *child);

static void _calculate_proc_stack(process_t *proc, size_t *stack_size, virt_addr_t *stack_top);
static void _calculate_proc_heap(process_t *proc, size_t *heap_size, virt_addr_t *heap_bottom);

static error_t _allocate_lot_of_physical_pages_atomically(int num_pages, phys_addr_t **addresses_arr);
static error_t _release_lot_of_physical_pages_atomically(phys_addr_t *addresses_arr, int num_pages);
static error_t _region_allocate_and_map(mem_region_t *reg, page_dir_t page_dir);
static void    _region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir);
static error_t _region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_dir, mem_region_t *src_reg, page_dir_t src_dir);

static error_t _allocate_and_map_stack_region(process_t *proc, size_t size, virt_addr_t stack_top);
static error_t _allocate_and_map_heap_region(process_t *proc, size_t size, virt_addr_t heap_base);
static error_t _allocate_and_map_elf_segment(process_t *proc, elf_loadable_segment_t *seg);
static error_t _load_elf_segment_page_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, int page_no, phys_addr_t page_addr);
static error_t _allocate_and_load_elf_segment_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg);
static error_t _allocate_and_load_elf_segments_from_file(process_t *proc, open_file_t *elf);

static error_t _duplicate_memory_region_if_needed(mem_region_t *dest_reg, page_dir_t dest_dir, mem_region_t *src_reg, page_dir_t src_dir);
static error_t _allocate_and_initialize_all_regions_for_elf(process_t *proc, open_file_t *elf);
static error_t _duplicate_all_memory_regions_from_process(process_t *dest, process_t *src);
static error_t _unmap_and_release_all_regions_of_process(process_t *proc);
static error_t _create_base_process_v2(page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr);


// ----------------------------------------------------------

static pid_t last_pid = 0;
static lock_t pid_lock = 0;

static int kernel_stacks_count = 0; 
static lock_t kernel_stacks_lock;

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = ++last_pid;
    mutex_release(&pid_lock);
    return id;
}

static int next_kernel_stack_no() {
    ASSERT(kernel_stacks_count < 15); // we shall not need more than a handful

    mutex_acquire(&kernel_stacks_lock);
    int no = kernel_stacks_count;
    kernel_stacks_count++;
    mutex_release(&kernel_stacks_lock);
    return no;
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

static void _calculate_proc_stack(process_t *proc, size_t *stack_size, virt_addr_t *stack_top) {
    ASSERT(proc != NULL);
    ASSERT(stack_top != NULL);
    ASSERT(stack_size != NULL);

    if (proc_is_kernel_proc(proc)) {
        size_t sz = 64 * KB;
        // put all kernel stacks near top of kernel, to allow for heap growth
        *stack_size = sz;
        *stack_top = vmm_get_kernel_top_address() - (sz * next_kernel_stack_no());
    } else {
        // put user stack at a high enough address
        *stack_size = 1 * MB;
        *stack_top = (2 * GB) - (*stack_size);
    }

    ASSERT(vmm_is_page_aligned(*stack_top));
    ASSERT(vmm_is_page_aligned(*stack_size));
}

static void _calculate_proc_heap(process_t *proc, size_t *heap_size, virt_addr_t *heap_bottom) {
    ASSERT(proc != NULL);
    ASSERT(heap_bottom != NULL);
    ASSERT(heap_size != NULL);

    if (proc_is_kernel_proc(proc)) {
        // kernel should not use this one, it already has heap
        *heap_bottom = 0;
        *heap_size = 0;
    } else {
        // go above all elf segments
        virt_addr_t addr = 128 * MB;

        for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
            mem_region_t *section = &proc->memory.elf_sections[i];
            virt_addr_t region_end = vmm_round_up(section->address + section->size);
            addr = max(addr, region_end);
        }
        
        *heap_bottom = addr;
        *heap_size = 64 * KB; // this can grow anyway
    }

    ASSERT(vmm_is_page_aligned(*heap_bottom));
    ASSERT(vmm_is_page_aligned(*heap_size));
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
        memset((void *)addr, 0, vmm_page_size());
        arr[page] = addr;
    }

    if (failed) {
        for (int page = 0; page < num_pages; page++)
            if (arr[page])
                pmm_free_physical_page(arr[page]);
        kfree(arr);
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

    bool user = (reg->flags & REGION_USER_ACCESSIBLE) != 0;
    bool writable = (reg->flags & REGION_WRITE_ENABLE) != 0;
    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_page(reg->address + page * vmm_page_size(), pages_arr[page], page_dir, user, writable);
        if (err) {
            while (--page >= 0) vmm_unmap_page(reg->address + page * vmm_page_size(), page_dir);
            _release_lot_of_physical_pages_atomically(pages_arr, num_pages);
            return err;
        }
    }
    
    kfree(pages_arr);
    return OK;
}

static void _region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir) {
    ASSERT(reg != NULL);
    ASSERT(page_dir > 0);
    ASSERT(vmm_is_page_aligned(reg->address));
    ASSERT(vmm_is_page_aligned(reg->size));

    int num_pages = reg->size / vmm_page_size();
    virt_addr_t vaddr = reg->address;
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t paddr = vmm_resolve(vaddr, page_dir);

        vmm_unmap_page(vaddr, page_dir);
        if (paddr != 0)
            pmm_free_physical_page(paddr);
        
        vaddr += vmm_page_size();
    }

    *reg = mem_region_empty();
}

static error_t _region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_dir, mem_region_t *src_reg, page_dir_t src_dir) {
    ASSERT(dest_reg != NULL);
    ASSERT(dest_reg->address > 0);
    ASSERT(dest_reg->size > 0);
    ASSERT(vmm_is_page_aligned(dest_reg->size));
    ASSERT(dest_dir > 0);
    ASSERT(src_reg != NULL);
    ASSERT(src_reg->address > 0);
    ASSERT(src_reg->size > 0);
    ASSERT(vmm_is_page_aligned(src_reg->size));
    ASSERT(src_dir > 0);

    error_t err = OK;
    bool page1_mapped = false;
    bool page2_mapped = false;

    // we map the physical pages to a copy area, using whatever CR3 we currently have.
    page_dir_t work_pd = vmm_get_current_page_dir();
    int num_pages = src_reg->size / vmm_page_size();

    for (int i = 0; i < num_pages; i++) {
        phys_addr_t dest_paddr = vmm_resolve(dest_reg->address + i * vmm_page_size(), dest_dir);
        phys_addr_t src_paddr = vmm_resolve(src_reg->address + i * vmm_page_size(), src_dir);
        if (dest_paddr == 0 || src_paddr == 0) return ERR_INVALID_ARGS;

        err = vmm_map_page(vmm_get_kernel_copy_area1(), dest_paddr, work_pd, false, true);
        if (err) goto exit;
        page1_mapped = true;

        err = vmm_map_page(vmm_get_kernel_copy_area2(), src_paddr, work_pd, false, false);
        if (err) goto exit;
        page2_mapped = true;

        memcpy((void *)vmm_get_kernel_copy_area1(), (void *)vmm_get_kernel_copy_area2(), vmm_page_size());
        
        vmm_unmap_page(vmm_get_kernel_copy_area1(), work_pd);
        page1_mapped = false;

        vmm_unmap_page(vmm_get_kernel_copy_area2(), work_pd);
        page2_mapped = false;
    }

exit:
    if (page1_mapped) vmm_unmap_page(vmm_get_kernel_copy_area1(), work_pd);
    if (page2_mapped) vmm_unmap_page(vmm_get_kernel_copy_area2(), work_pd);
    return err;
}

static error_t _allocate_and_map_stack_region(process_t *proc, size_t size, virt_addr_t stack_top) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(stack_top > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(stack_top));

    // suggestion: add two guard pages, one for overflow and one for underflow.

    region_flags_t flags = REGION_USAGE_STACK | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(stack_top - size, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.stack = reg;
    return OK;
}

static error_t _allocate_and_map_heap_region(process_t *proc, size_t size, virt_addr_t heap_base) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(heap_base > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(heap_base));

    // suggestion: add one guard page for heap overflow

    region_flags_t flags = REGION_USAGE_HEAP | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(heap_base, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.heap = reg;
    return OK;
}

static error_t _allocate_and_map_elf_segment(process_t *proc, elf_loadable_segment_t *seg) {
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

static error_t _load_elf_segment_page_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, int page_no, phys_addr_t page_addr) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    ASSERT(seg != NULL);
    ASSERT(page_addr != 0);

    error_t err = OK;

    // we map the physical page to a copy area, using whatever CR3 we currently have.
    // the kernel is running on ring 0, so it does have access to it, no matter the contents of CR3.
    page_dir_t work_pd = vmm_get_current_page_dir();
    vmm_map_page(vmm_get_kernel_copy_area1(), page_addr, work_pd, false, true);

    size_t page_gap = page_no > 0 ? 0 : seg->address_in_mem - vmm_round_down(seg->address_in_mem);
    off_t offset_in_file = seg->offset_in_file + page_no * vmm_page_size();
    size_t remaining_bytes = seg->size_in_file - page_no * vmm_page_size();
    size_t bytes_to_read = min(remaining_bytes, vmm_page_size() - page_gap);

    memset((void *)vmm_get_kernel_copy_area1(), 0, vmm_page_size());

    off_t new_off = vfs_seek(elf, offset_in_file, SEEK_SET);
    if (new_off != offset_in_file) { err = ERR_READING_FILE; goto exit; }
    ssize_t read = vfs_read(elf, (void *)(vmm_get_kernel_copy_area1() + page_gap), bytes_to_read);
    if (read < 0) { err = (error_t)read; goto exit; }
    if ((size_t)read < bytes_to_read) { err = ERR_READING_FILE; goto exit; }

exit:
    vmm_unmap_page(vmm_get_kernel_copy_area1(), work_pd);
    return err;
}

static error_t _allocate_and_load_elf_segment_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    ASSERT(seg != NULL);

    log_trace("loading elf segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c",
        seg->offset_in_file, seg->size_in_file, seg->address_in_mem, seg->size_in_mem, seg->writable ? 'W' : '-', seg->executable ? 'X' : '-');

    // this allocates and maps, to prepare the memory for loading
    error_t err = _allocate_and_map_elf_segment(proc, seg);
    if (err) goto exit;

    // now we need to take it one by one.
    mem_region_t *reg = &proc->memory.elf_sections[proc->memory.elf_sections_count - 1];
    int num_pages = reg->size / vmm_page_size();
    page_dir_t pd = proc->memory.page_dir;
    for (int page = 0; page < num_pages; page++) {
        virt_addr_t vaddr = reg->address + page * vmm_page_size();
        phys_addr_t paddr = vmm_resolve(vaddr, pd);

        // load into physical page, via temp mapping onto kernel copy area
        err = _load_elf_segment_page_from_file(proc, elf, seg, page, paddr);
        if (err) goto exit;
    }

    return OK;
exit:
    return err;
}

static error_t _allocate_and_load_elf_segments_from_file(process_t *proc, open_file_t *elf) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);

    elf_loadable_segment_t *segments_arr = NULL;
    error_t err = OK;

    int headers = 0;
    err = elf_get_program_headers_count(elf, &headers);
    if (err) goto exit;
    if (headers <= 0 || headers > MAX_PROCESS_ELF_SECTIONS)
        return ERR_CORRUPTION_DETECTED;

    segments_arr = kmalloc(sizeof(elf_loadable_segment_t) * headers);
    if (segments_arr == NULL) { err = ERR_NO_MEMORY; goto exit; }

    err = elf_get_program_headers_info(elf, segments_arr, headers);
    if (err) goto exit;

    for (int i = 0; i < headers; i++) {
        err = _allocate_and_load_elf_segment_from_file(proc, elf, &segments_arr[i]);
        if (err) goto exit;
    }

    // let's not forget this!
    err = elf_get_entry_point(elf, (virt_addr_t *)&proc->entry_point);
    if (err) goto exit;
    err = OK;

exit:
    if (segments_arr != NULL) kfree(segments_arr);
    return err;
}

static error_t _duplicate_memory_region_if_needed(mem_region_t *dest_reg, page_dir_t dest_dir, mem_region_t *src_reg, page_dir_t src_dir) {
    error_t err = OK;
    bool region_allocated = false;

    if (mem_region_is_empty(src_reg))
        return OK;

    *dest_reg = *src_reg; // copy values

    err = _region_allocate_and_map(dest_reg, dest_dir);
    if (err) goto error;
    region_allocated = true;

    err = _region_copy_contents(dest_reg, dest_dir, src_reg, src_dir);
    if (err) goto error;

    return OK;
error:
    if (region_allocated)
        _region_unmap_and_release(dest_reg, dest_dir);
    *dest_reg = mem_region_empty(); // clear
    return err;
}

static error_t _allocate_and_initialize_all_regions_for_elf(process_t *proc, open_file_t *elf) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);

    error_t err = OK;

    err = _allocate_and_load_elf_segments_from_file(proc, elf);
    if (err) goto exit;

    size_t heap_size = 0;
    virt_addr_t heap_addr = 0;
    _calculate_proc_heap(proc, &heap_size, &heap_addr);
    err = _allocate_and_map_heap_region(proc, heap_size, heap_addr);
    if (err) goto exit;

    size_t stack_size = 0;
    virt_addr_t stack_top = 0;
    _calculate_proc_stack(proc, &stack_size, &stack_top);
    err = _allocate_and_map_stack_region(proc, stack_size, stack_top);
    if (err) goto exit;

exit:
    return err;
}

static error_t _duplicate_all_memory_regions_from_process(process_t *dest, process_t *src) {
    error_t err = OK;

    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        err = _duplicate_memory_region_if_needed(&dest->memory.elf_sections[i], dest->memory.page_dir, &src->memory.elf_sections[i], src->memory.page_dir);
        if (err) goto exit;
    }
    dest->memory.elf_sections_count = src->memory.elf_sections_count;
    
    err = _duplicate_memory_region_if_needed(&dest->memory.stack, dest->memory.page_dir, &src->memory.stack, src->memory.page_dir);
    if (err) goto exit;

    err = _duplicate_memory_region_if_needed(&dest->memory.heap, dest->memory.page_dir, &src->memory.heap, src->memory.page_dir);
    if (err) goto exit;

exit:
    return err;
}

static error_t _unmap_and_release_all_regions_of_process(process_t *proc) {
    page_dir_t page_dir = proc->memory.page_dir;

    for (int i = 0; i < proc->memory.elf_sections_count; i++) {
        mem_region_t *reg = &proc->memory.elf_sections[i];
        if (!mem_region_is_empty(reg))
            _region_unmap_and_release(reg, page_dir);
    }

    if (!mem_region_is_empty(&proc->memory.heap))
        _region_unmap_and_release(&proc->memory.heap, page_dir);

    if (!mem_region_is_empty(&proc->memory.stack))
        _region_unmap_and_release(&proc->memory.stack, page_dir);
    
    return OK;
}

static error_t _create_base_process_v2(page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr) {
    ASSERT(pd != 0);
    ASSERT(name != NULL);
    ASSERT(proc_ptr != NULL);

    process_t *proc = NULL;
    error_t err = OK;
    
    proc = (process_t *)kmalloc(sizeof(process_t));
    if (proc == NULL) { err = ERR_NO_MEMORY; goto failed; }
    memset(proc, 0, sizeof(process_t));
    
    proc->pid = next_pid();
    if (parent != NULL) {
        proc->parent = parent;
        proc_add_child(parent, proc);
    }
    proc->priority = priority;
    proc->name = kstrdup(name);
    if (proc->name == NULL) { err = ERR_NO_MEMORY; goto failed; }

    // set the cwd
    // set the open files (stdin, stdout, stderr)
    // push env and argv

    *proc_ptr = proc;
    return OK;

failed:
    if (proc && proc->name) kfree(proc->name);
    if (proc) kfree(proc);
    return err;
}

error_t create_kernel_process_v2(const char *name, uintptr_t function_to_call, proc_priority_t priority, process_t **proc_ptr) {
    process_t *proc;
    error_t err = _create_base_process_v2(vmm_get_kernel_page_directory(), NULL, priority, name, &proc);
    if (err) return err;

    size_t stack_size = 0;
    virt_addr_t stack_top = 0;
    _calculate_proc_stack(proc, &stack_size, &stack_top);
    err = _allocate_and_map_stack_region(proc, stack_size, stack_top);
    if (err) panic("failed allocating and mapping stack for kernel task");

    // there's little more to do here, isn't it...
    proc->entry_point = function_to_call;

    *proc_ptr = proc;
    return OK;
}

error_t create_user_process_v2(process_t *parent, const char *file_path, proc_priority_t priority, process_t **proc_ptr) {
    ASSERT(file_path != 0); 
    ASSERT(proc_ptr != 0);

    error_t err = OK;
    open_file_t *elf = NULL;
    process_t *proc = NULL;
    page_dir_t pd = 0;

    err = vfs_open(file_path, 0, &elf);
    if (err) goto failed;
    err = elf_verify_executable(elf); // verify early for better recovery
    if (err) goto failed;

    pd = vmm_create_page_directory(true);
    
    err = _create_base_process_v2(pd, parent, priority, file_path, &proc);
    if (err) goto failed;
    pd = 0; // from now on, the process shall destroy the PD
    
    err = _allocate_and_initialize_all_regions_for_elf(proc, elf);
    if (err) goto failed;

    vfs_close(elf);
    *proc_ptr = proc;
    return OK;

failed:
    if (pd) vmm_destroy_page_directory(pd);
    if (elf) vfs_close(elf);
    if (proc) proc_destroy(proc);
    return err;
}

error_t replace_user_process_v2(process_t *proc, const char *file_path) {
    ASSERT(proc != NULL);
    ASSERT(file_path != NULL);

    error_t err = OK;
    open_file_t *elf;

    err = vfs_open(file_path, 0, &elf);
    if (err) goto failed;
    err = elf_verify_executable(elf); // verify early for better recovery
    if (err) goto failed;

    // we won't be needing those any more
    err = _unmap_and_release_all_regions_of_process(proc);
    if (err) goto failed;

    // then initialize new ones all over again
    err = _allocate_and_initialize_all_regions_for_elf(proc, elf);
    if (err) goto failed;

    vfs_close(elf);

    // we would never return, right? we should just restart execution...
    return OK;

failed:
    if (elf) vfs_close(elf);
    // we may be in a pretty unrunnable state! 
    // real kernels solve this by loading new image and if success, swap. not blindly destroying first
    return err;
}

error_t fork_user_process_v2(process_t *parent, process_t **proc_ptr) {
    ASSERT(parent != NULL);
    ASSERT(proc_ptr != NULL);

    error_t err = OK;
    process_t *child = NULL;
    page_dir_t pd = 0;

    pd = vmm_create_page_directory(true);
    
    err = _create_base_process_v2(pd, parent, parent->priority, parent->name, &child);
    if (err) goto failed;
    pd = 0; // from now on, the process shall destroy the PD
    
    err = _duplicate_all_memory_regions_from_process(child, parent);
    if (err) goto failed;

    // we'll need a few more things, but this is looking better

    *proc_ptr = child;
    return OK;

failed:
    if (pd) vmm_destroy_page_directory(pd);
    if (child) proc_destroy(child);
    return err;
}


// --------------- original kernel's attempt ---------------------------------
// original proc create code below
// ---------------------------------------------------------------------------

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
    
    if (proc->parent)
        proc_remove_child(proc->parent, proc);

    if (proc->name != NULL)
        kfree(proc->name);

    if (proc->allocated_kernel_stack != 0)
        kfree(proc->allocated_kernel_stack);

    _unmap_and_release_all_regions_of_process(proc);

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

// ---------------------------------------------------------

//TODO: finish the log formatter for proc
void proc_log_formatter(log_write_stream_t *stream, va_list args) {
    process_t *proc = va_arg(args, process_t *);

    // ...
}
