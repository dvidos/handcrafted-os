#include "process.h"
#include "../../arch/gdt.h"
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

MODULE("PROC_CREATE", LOG_LEVEL_DEBUG);


static pid_t   next_pid();
static int     next_kernel_stack_no();
static void    proc_add_child(process_t *parent, process_t *child);
static void    proc_remove_child(process_t *parent, process_t *child);

static void _calculate_proc_user_stack(process_t *proc, size_t *stack_size, virt_addr_t *stack_top);
static void _calculate_proc_user_heap(process_t *proc, size_t *heap_size, virt_addr_t *heap_bottom);

static error_t _allocate_lot_of_physical_pages_atomically(int num_pages, phys_addr_t **addresses_arr);
static error_t _release_lot_of_physical_pages_atomically(phys_addr_t *addresses_arr, int num_pages);
static error_t _region_allocate_and_map(mem_region_t *reg, page_dir_t page_dir);
static void    _region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir);
static error_t _region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd);

static error_t _allocate_and_map_user_stack_region(process_t *proc, size_t size, virt_addr_t stack_top);
static error_t _allocate_and_map_user_heap_region(process_t *proc, size_t size, virt_addr_t heap_base);
static error_t _allocate_and_map_elf_segment(process_t *proc, int section_no, elf_loadable_segment_t *seg);
static error_t _load_elf_segment_page_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, int page_no, phys_addr_t page_addr, char *page_buffer);
static error_t _allocate_and_load_elf_segment_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, char *page_buffer);
static error_t _allocate_and_load_elf_segments_from_file(process_t *proc, open_file_t *elf);

static error_t _duplicate_memory_region_if_needed(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd);
static error_t _allocate_and_initialize_all_regions_for_elf(process_t *proc, open_file_t *elf);
static error_t _duplicate_all_memory_regions_from_process(process_t *dest, process_t *src);
static error_t _unmap_and_release_all_regions_of_process(process_t *proc);
static error_t _create_base_process_v2(bool is_user_proc, page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr);


// ----------------------------------------------------------

static pid_t last_pid = 0;
static lock_t pid_lock = 0;

static int kernel_stacks_count = 0; 
static lock_t kernel_stacks_lock;

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = last_pid++;
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

static void _calculate_proc_user_stack(process_t *proc, size_t *stack_size, virt_addr_t *stack_top) {
    ASSERT(proc != NULL);
    ASSERT(stack_top != NULL);
    ASSERT(stack_size != NULL);
    log_trace("_calculate_proc_user_stack(proc=%p)", proc);

    // put user stack at a high enough address
    *stack_size = 16 * KB;
    *stack_top = (2 * GB) - (*stack_size);

    ASSERT(vmm_is_page_aligned(*stack_top));
    ASSERT(vmm_is_page_aligned(*stack_size));
}

static void _calculate_proc_user_heap(process_t *proc, size_t *heap_size, virt_addr_t *heap_bottom) {
    ASSERT(proc != NULL);
    ASSERT(heap_bottom != NULL);
    ASSERT(heap_size != NULL);
    log_trace("_calculate_proc_user_heap(proc=%p)", proc);

    // go above all elf segments
    virt_addr_t addr = 128 * MB;

    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        mem_region_t *section = &proc->memory.elf_sections[i];
        virt_addr_t region_end = vmm_round_up(section->address + section->size);
        addr = max(addr, region_end);
    }
    
    *heap_bottom = addr;
    *heap_size = 64 * KB; // this can grow anyway

    ASSERT(vmm_is_page_aligned(*heap_bottom));
    ASSERT(vmm_is_page_aligned(*heap_size));
}

// ----------------------------------------------------------------

static error_t _allocate_lot_of_physical_pages_atomically(int num_pages, phys_addr_t **addresses_arr) {
    ASSERT(num_pages > 0);
    ASSERT(addresses_arr != NULL);
    log_trace("_allocate_lot_of_physical_pages_atomically(num=%d)", num_pages);

    phys_addr_t *arr = kmalloc(sizeof(phys_addr_t) * num_pages);
    if (arr == NULL) return traceable(ERR_NO_MEMORY);
    memset(arr, 0, sizeof(virt_addr_t) * num_pages);

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
        kfree(arr);
        return traceable(ERR_NO_MEMORY);
    }

    *addresses_arr = arr;
    log_trace("_allocate_lot_of_physical_pages_atomically() done");
    return OK;
}

static error_t _release_lot_of_physical_pages_atomically(phys_addr_t *addresses_arr, int num_pages) {
    ASSERT(addresses_arr != NULL);
    ASSERT(num_pages > 0);
    log_trace("_release_lot_of_physical_pages_atomically(addr=%p, num=%d)", addresses_arr, num_pages);

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
    log_trace("_region_allocate_and_map(reg=%p, page_dir=%x)", reg, page_dir);
    
    phys_addr_t *pages_arr;
    int num_pages = vmm_pages_for_size(reg->size);
    error_t err = _allocate_lot_of_physical_pages_atomically(num_pages, &pages_arr);
    if (err) return err;

    bool user = (reg->flags & REGION_USER_ACCESSIBLE) != 0;
    bool writable = (reg->flags & REGION_WRITE_ENABLE) != 0;
    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_page_to_pd(reg->address + page * vmm_page_size(), pages_arr[page], user, writable, page_dir);
        if (err) {
            log_warn("error while mapping page %d/%d, will unmap all and release physical pages", page, num_pages);
            while (--page >= 0) vmm_unmap_page_from_pd(reg->address + page * vmm_page_size(), page_dir);
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
    log_trace("_region_unmap_and_release(reg=%p, page_dir=%x)", reg, page_dir);

    int num_pages = reg->size / vmm_page_size();
    virt_addr_t vaddr = reg->address;
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t paddr = vmm_resolve(vaddr, page_dir);

        vmm_unmap_page_from_pd(vaddr, page_dir);
        if (paddr != 0)
            pmm_free_physical_page(paddr);
        
        vaddr += vmm_page_size();
    }

    *reg = mem_region_empty();
}

static error_t _region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd) {
    ASSERT(dest_reg != NULL);
    ASSERT(dest_reg->address > 0);
    ASSERT(dest_reg->size > 0);
    ASSERT(vmm_is_page_aligned(dest_reg->size));
    ASSERT(dest_pd > 0);
    ASSERT(src_reg != NULL);
    ASSERT(src_reg->address > 0);
    ASSERT(src_reg->size > 0);
    ASSERT(vmm_is_page_aligned(src_reg->size));
    ASSERT(src_pd > 0);
    log_trace("_region_copy_contents(dest_reg=%p, dest_pd=%x, src_reg=%p, src_pd=%x)", dest_reg, dest_pd, src_reg, src_pd);

    error_t err = OK;

    // we map the physical pages to a copy area, using whatever CR3 we currently have.
    int num_pages = src_reg->size / vmm_page_size();

    for (int i = 0; i < num_pages; i++) {
        phys_addr_t dest_paddr = vmm_resolve(dest_reg->address + i * vmm_page_size(), dest_pd);
        phys_addr_t src_paddr = vmm_resolve(src_reg->address + i * vmm_page_size(), src_pd);
        if (dest_paddr == 0 || src_paddr == 0)
            return traceable(ERR_INVALID_ARGS);

        vmm_physpg_copy(dest_paddr, src_paddr);
    }

    return traceable(err);
}

static error_t _allocate_and_map_user_stack_region(process_t *proc, size_t size, virt_addr_t stack_top) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(stack_top > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(stack_top));
    log_trace("_allocate_and_map_user_stack_region(proc=%p, size=%u, top=%x)", proc, size, stack_top);

    // suggestion: add two guard pages, one for overflow and one for underflow.

    region_flags_t flags = REGION_USAGE_STACK | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(stack_top - size, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.user_stack = reg;

    return OK;
}

static error_t _allocate_and_map_user_heap_region(process_t *proc, size_t size, virt_addr_t heap_base) {
    ASSERT(proc != NULL);
    ASSERT(size > 0);
    ASSERT(heap_base > 0);
    ASSERT(vmm_is_page_aligned(size));
    ASSERT(vmm_is_page_aligned(heap_base));
    log_trace("_allocate_and_map_user_heap_region(proc=%p, size=%u, base=%x)", proc, size, heap_base);

    // suggestion: add one guard page for heap overflow

    region_flags_t flags = REGION_USAGE_HEAP | REGION_WRITE_ENABLE |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    mem_region_t reg = mem_region_of(heap_base, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.user_heap = reg;
    return OK;
}

static error_t _allocate_and_map_elf_segment(process_t *proc, int section_no, elf_loadable_segment_t *seg) {
    ASSERT(proc != NULL);
    ASSERT(section_no < MAX_PROCESS_ELF_SECTIONS);
    ASSERT(seg != NULL);
    log_trace("_allocate_and_map_elf_segment(proc=%p, section=%d, seg=%p)", proc, section_no, seg);

    virt_addr_t addr = vmm_round_down(seg->address_in_mem);
    size_t size = vmm_round_up(seg->address_in_mem + seg->size_in_mem) - addr;
    region_flags_t flags = REGION_USAGE_ELF |
        (seg->writable ? REGION_WRITE_ENABLE : REGION_READ_ONLY) |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    
    mem_region_t reg = mem_region_of(addr, size, flags);
    error_t err = _region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.elf_sections[section_no] = reg;
    return OK;
}

static error_t _load_elf_segment_page_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, int page_no, phys_addr_t page_addr, char *page_buffer) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    ASSERT(seg != NULL);
    ASSERT(page_addr != 0);
    log_trace("_load_elf_segment_page_from_file(proc=%p, elf=%p, seg=%p, page_no=%d, page_addr=%p)", proc, elf, seg, page_no, page_addr);

    error_t err = OK;
    page_dir_t curr_pd = vmm_get_current_page_dir();
    size_t page_gap = page_no > 0 ? 0 : seg->address_in_mem - vmm_round_down(seg->address_in_mem);
    off_t offset_in_file = seg->offset_in_file + page_no * vmm_page_size();
    size_t remaining_bytes = seg->size_in_file - page_no * vmm_page_size();
    size_t bytes_to_read = min(remaining_bytes, vmm_page_size() - page_gap);

    memset(page_buffer, 0, vmm_page_size());

    off_t new_off = vfs_seek(elf, offset_in_file, SEEK_SET);
    if (new_off != offset_in_file) { err = ERR_READING_FILE; goto exit; }
    ssize_t read = vfs_read(elf, page_buffer + page_gap, bytes_to_read);
    if (read < 0) { err = (error_t)read; goto exit; }
    if ((size_t)read < bytes_to_read) { err = ERR_READING_FILE; goto exit; }

    vmm_physpg_write(page_addr, 0, page_buffer, vmm_page_size());

    // log_debug("ELF buffer contents for page (page_no=%d, gap=%d, to_read=%d):", page_no, page_gap, bytes_to_read);
    // log_debug_hex(page_buffer, bytes_to_read, page_gap);

exit:
    return traceable(err);
}

static error_t _allocate_and_load_elf_segment_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, char *page_buffer) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    ASSERT(seg != NULL);
    log_trace("_allocate_and_load_elf_segment_from_file(proc=%p, elf=%p, seg=%p, buffer=%p)", proc, elf, seg, page_buffer);


    log_debug("loading elf segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c",
        seg->offset_in_file, seg->size_in_file, seg->address_in_mem, seg->size_in_mem, seg->writable ? 'W' : '-', seg->executable ? 'X' : '-');

    int section_no = proc_count_elf_sections(proc);
    ASSERT(section_no >= 0 && section_no < MAX_PROCESS_ELF_SECTIONS);

    // this allocates and maps, to prepare the memory for loading
    error_t err = _allocate_and_map_elf_segment(proc, section_no, seg);
    if (err) goto exit;

    // now we need to take it page by page
    mem_region_t *reg = &proc->memory.elf_sections[section_no];
    int num_pages = reg->size / vmm_page_size();
    page_dir_t pd = proc->memory.page_dir;

    for (int page = 0; page < num_pages; page++) {
        virt_addr_t vaddr = reg->address + page * vmm_page_size();
        phys_addr_t paddr = vmm_resolve(vaddr, pd);

        // load into physical page, via temp mapping onto kernel copy area
        err = _load_elf_segment_page_from_file(proc, elf, seg, page, paddr, page_buffer);
        if (err) goto exit;
    }

    return OK;
exit:
    return traceable(err);
}

static error_t _prepare_trap_frame_for_new_user_process(process_t *proc, uint32_t elf_entry_point) {

    // this function for user processes
    ASSERT(proc_is_user_proc(proc));
    ASSERT(proc->memory.kernel_stack.address != 0);
    ASSERT(proc->memory.kernel_stack.size != 0);

    // trap frame is located on the kernel_stack, not the user_stack
    trap_frame_t *tf = (trap_frame_t *)(proc->memory.kernel_stack.address + proc->memory.kernel_stack.size - sizeof(trap_frame_t));

    memset(tf, 0, sizeof(trap_frame_t));
    tf->eip = elf_entry_point;
    tf->cs  = USER_CODE_SEGMENT;
    tf->user_esp = proc->memory.user_stack.address + proc->memory.user_stack.size; // this is where CPU will return in ring 3, after 'iret'
    tf->ss  = USER_DATA_SEGMENT;
    tf->eflags = 0x202;   // interrupts enabled, this is important

    tf->ds = USER_DATA_SEGMENT;
    tf->es = USER_DATA_SEGMENT;
    tf->fs = USER_DATA_SEGMENT;
    tf->gs = USER_DATA_SEGMENT;

    return OK;
}

static error_t _allocate_and_load_elf_segments_from_file(process_t *proc, open_file_t *elf) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    log_trace("_allocate_and_load_elf_segments_from_file(proc=%p, elf=%p)", proc, elf);

    elf_loadable_segment_t *segments_arr = NULL;
    char *page_buffer = NULL;
    error_t err = OK;

    int headers = 0;
    err = elf_get_program_headers_count(elf, &headers);
    if (err) goto exit;
    if (headers <= 0 || headers > MAX_PROCESS_ELF_SECTIONS)
        return ERR_CORRUPTION_DETECTED;

    segments_arr = kmalloc(sizeof(elf_loadable_segment_t) * headers);
    if (segments_arr == NULL) { err = ERR_NO_MEMORY; goto exit; }

    page_buffer = kmalloc(vmm_page_size());
    if (page_buffer == NULL) { err = ERR_NO_MEMORY; goto exit; }

    err = elf_get_program_headers_info(elf, segments_arr, headers);
    if (err) goto exit;
    for (int i = 0; i < headers; i++) log_debug_fmt(elf_segment_formatter, "segment from elf:", &segments_arr[i]);

    for (int i = 0; i < headers; i++) {
        err = _allocate_and_load_elf_segment_from_file(proc, elf, &segments_arr[i], page_buffer);
        if (err) goto exit;
    }
    
    virt_addr_t elf_entry_point = 0;
    err = elf_get_entry_point(elf, &elf_entry_point);
    if (err) goto exit;
    log_debug("Elf entry point is 0x%08x", elf_entry_point);
    
    // prepare a trap frame for introducing the task
    _prepare_trap_frame_for_new_user_process(proc, elf_entry_point);

    err = OK;

exit:
    if (page_buffer != NULL) kfree(page_buffer);
    if (segments_arr != NULL) kfree(segments_arr);
    return traceable(err);
}

static error_t _duplicate_memory_region_if_needed(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd) {
    ASSERT(dest_reg != NULL);
    ASSERT(dest_pd != 0);
    log_trace("_duplicate_memory_region_if_needed(dest_reg=%p, dest_pd=%x, src_reg=%p, src_pd=%x)", dest_reg, dest_pd, src_reg, src_pd);

    error_t err = OK;
    bool region_allocated = false;

    if (mem_region_is_empty(src_reg))
        return OK;

    *dest_reg = *src_reg; // copy values

    err = _region_allocate_and_map(dest_reg, dest_pd);
    if (err) goto error;
    region_allocated = true;

    err = _region_copy_contents(dest_reg, dest_pd, src_reg, src_pd);
    if (err) goto error;

    return OK;
error:
    if (region_allocated)
        _region_unmap_and_release(dest_reg, dest_pd);
    *dest_reg = mem_region_empty(); // clear
    return traceable(err);
}

static error_t _allocate_and_initialize_all_regions_for_elf(process_t *proc, open_file_t *elf) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    log_trace("_allocate_and_initialize_all_regions_for_elf(proc=%p, elf=%p)", proc, elf);

    error_t err = OK;

    size_t stack_size = 0;
    virt_addr_t stack_top = 0;
    _calculate_proc_user_stack(proc, &stack_size, &stack_top);
    err = _allocate_and_map_user_stack_region(proc, stack_size, stack_top);
    if (err) goto exit;

    err = _allocate_and_load_elf_segments_from_file(proc, elf);
    if (err) goto exit;

    size_t heap_size = 0;
    virt_addr_t heap_addr = 0;
    _calculate_proc_user_heap(proc, &heap_size, &heap_addr);
    err = _allocate_and_map_user_heap_region(proc, heap_size, heap_addr);
    if (err) goto exit;

exit:
    return traceable(err);
}

static error_t _duplicate_all_memory_regions_from_process(process_t *dest, process_t *src) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    log_trace("_duplicate_all_memory_regions_from_process(dest=%p, src=%p)", dest, src);

    error_t err = OK;

    err = _duplicate_memory_region_if_needed(&dest->memory.user_stack, dest->memory.page_dir, &src->memory.user_stack, src->memory.page_dir);
    if (err) goto exit;

    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        err = _duplicate_memory_region_if_needed(&dest->memory.elf_sections[i], dest->memory.page_dir, &src->memory.elf_sections[i], src->memory.page_dir);
        if (err) goto exit;
    }
    
    err = _duplicate_memory_region_if_needed(&dest->memory.user_heap, dest->memory.page_dir, &src->memory.user_heap, src->memory.page_dir);
    if (err) goto exit;

exit:
    return traceable(err);
}

static error_t _unmap_and_release_all_regions_of_process(process_t *proc) {
    ASSERT(proc != NULL);
    log_trace("_unmap_and_release_all_regions_of_process(proc=%p)", proc);

    page_dir_t page_dir = proc->memory.page_dir;

    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        mem_region_t *reg = &proc->memory.elf_sections[i];
        if (mem_region_is_empty(reg))
            continue;

        _region_unmap_and_release(reg, page_dir);
    }

    if (!mem_region_is_empty(&proc->memory.user_heap))
        _region_unmap_and_release(&proc->memory.user_heap, page_dir);

    if (!mem_region_is_empty(&proc->memory.user_stack))
        _region_unmap_and_release(&proc->memory.user_stack, page_dir);
    
    return OK;
}

static error_t _create_base_process_v2(bool is_user_proc, page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr) {
    ASSERT(pd != 0);
    ASSERT(name != NULL);
    ASSERT(proc_ptr != NULL);

    log_trace("_create_base_process_v2()");

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
    proc->is_user = is_user_proc;
    proc->priority = priority;
    proc->memory.page_dir = pd;

    proc->name = kstrdup(name);
    if (proc->name == NULL) { err = ERR_NO_MEMORY; goto failed; }

    // all processes (user & kernel) take a small kernel_stack, identity mapped
    proc->memory.kernel_stack.size = 4096;
    proc->memory.kernel_stack.address = (uintptr_t)kmalloc(proc->memory.kernel_stack.size);
    if (proc->memory.kernel_stack.address == 0) { err = ERR_NO_MEMORY; goto failed; }

    // this to be given to scheduler at each switch
    proc->memory.tss_esp0_value = (uint32_t)(proc->memory.kernel_stack.address + proc->memory.kernel_stack.size);
    // for first execution, we will place a trap_frame at the top of kernel stack, so set saved_esp accordingly
    proc->memory.saved_esp = (uint32_t)(proc->memory.kernel_stack.address + proc->memory.kernel_stack.size - sizeof(trap_frame_t));

    *proc_ptr = proc;
    return OK;

failed:
    if (proc && proc->name) kfree(proc->name);
    if (proc && proc->memory.kernel_stack.address) kfree((void *)proc->memory.kernel_stack.address);
    if (proc) kfree(proc);
    return traceable(err);
}

error_t process_v2_create_for_kernel(const char *name, uintptr_t function_to_call, proc_priority_t priority, process_t **proc_ptr) {
    log_trace("process_v2_create_for_kernel(name=%s)", name);

    process_t *proc;
    error_t err = _create_base_process_v2(false, vmm_get_kernel_page_directory(), NULL, priority, name, &proc);
    if (err) return err;

    size_t stack_size = 0;
    virt_addr_t stack_top = 0;
    _calculate_proc_user_stack(proc, &stack_size, &stack_top);
    err = _allocate_and_map_user_stack_region(proc, stack_size, stack_top);
    if (err) panic("failed allocating and mapping stack for kernel task");

    // there's little more to do here, isn't it...
    // TODO: i think we did not setup return address..
    // the stack should be mapped, maybe do it directly?
    // proc->memory.execution.return_Address = function_to_call?
    // GPT says we should have a small trampoline, where when the task returns, we just remove it from lists
    // but, since this will tie into creating tasks/threads, i leave it for later.
    proc->entry_point = function_to_call;

    *proc_ptr = proc;
    return OK;
}

error_t process_v2_create_for_spawn(process_t *parent, const char *file_path, proc_priority_t priority, process_t **proc_ptr) {
    ASSERT(file_path != 0); 
    ASSERT(proc_ptr != 0);
    log_trace("process_v2_create_for_spawn(parent=%p, file='%s')", parent, file_path);

    error_t err = OK;
    open_file_t *elf = NULL;
    process_t *proc = NULL;
    page_dir_t new_pd = 0;

    err = vfs_open(file_path, 0, &elf);
    if (err) goto failed;
    err = elf_verify_executable(elf); // verify early for better recovery
    if (err) goto failed;

    new_pd = vmm_create_page_directory(true);
    if (new_pd == 0) { err = ERR_NO_MEMORY; goto failed; }
    vmm_dump_page_directory(new_pd);
    
    err = _create_base_process_v2(true, new_pd, parent, priority, file_path, &proc);
    if (err) goto failed;
    new_pd = 0; // from now on, the process shall destroy the PD
    
    err = _allocate_and_initialize_all_regions_for_elf(proc, elf);
    if (err) goto failed;

    vfs_close(elf);
    *proc_ptr = proc;
    return OK;

failed:
    if (new_pd) vmm_destroy_page_directory(new_pd);
    if (elf) vfs_close(elf);
    if (proc) proc_destroy(proc);
    return traceable(err);
}

error_t process_v2_replace_for_exec(process_t *proc, const char *file_path) {
    ASSERT(proc != NULL);
    ASSERT(file_path != NULL);
    log_trace("process_v2_replace_for_exec(proc=%p, file='%s')", proc, file_path);

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
    return traceable(err);
}

error_t process_v2_create_for_fork(process_t *parent, process_t **proc_ptr) {
    ASSERT(parent != NULL);
    ASSERT(proc_ptr != NULL);
    log_trace("process_v2_create_for_fork(parent=%p)", parent);

    error_t err = OK;
    process_t *child = NULL;
    page_dir_t pd = 0;

    pd = vmm_create_page_directory(true);
    
    err = _create_base_process_v2(true, pd, parent, parent->priority, parent->name, &child);
    if (err) goto failed;
    pd = 0; // from now on, the process shall destroy the PD
    
    err = _duplicate_all_memory_regions_from_process(child, parent);
    if (err) goto failed;

    child->memory.saved_esp = parent->memory.saved_esp;
    // maybe we can influence the return value of the child here???

    // we'll need a few more things, but this is looking better
    ASSERT(child->memory.saved_esp != 0);

    *proc_ptr = child;
    return OK;

failed:
    if (pd) vmm_destroy_page_directory(pd);
    if (child) proc_destroy(child);
    return traceable(err);
}



// after a process has terminated, clean up resources
void proc_destroy(process_t *proc) {
    
    if (proc->parent)
        proc_remove_child(proc->parent, proc);

    if (proc->name != NULL)
        kfree(proc->name);

    _unmap_and_release_all_regions_of_process(proc);

    if (proc->memory.page_dir != 0 && proc->memory.page_dir != vmm_get_kernel_page_directory())
        vmm_destroy_page_directory(proc->memory.page_dir);

    if (proc->memory.kernel_stack.address != 0) {
        kfree((void *)proc->memory.kernel_stack.address);
        proc->memory.kernel_stack.address = 0;
    }
    if (proc->user_proc.executable_path != NULL) {
        kfree(proc->user_proc.executable_path);
        proc->user_proc.executable_path = NULL;
    }
    if (proc->user_proc.argv != NULL) {
        free_strvec(proc->user_proc.argv);
        proc->user_proc.argv = NULL;
    }
    if (proc->user_proc.envp != NULL) {
        free_strvec(proc->user_proc.envp);
        proc->user_proc.envp = NULL;
    }
    if (proc->curr_dir_path != NULL) {
        kfree(proc->curr_dir_path);
        proc->curr_dir_path = NULL;
    }

    // let's release all file handles as well.
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (proc->file_handles[i] == NULL)
            continue;
        
        open_files.release(proc->file_handles[i]);
    }
    
    // can't think (atm) of anything else to free
    kfree(proc);
}
