#include "process.h"
#include "../../arch/gdt.h"
#include "../procman/proclist.h"
#include "../procman/scheduler.h"
#include "../multitask.h"
#include "../../include/ctypes.h"
#include "../../include/macros.h"
#include "../../logger/logger.h"
#include "../../memory/kheap.h"
#include "../../memory/vmm.h"
#include "../../klib/string.h"
#include "../../klib/strvec.h"
#include "../../utils/assert.h"
#include "../elf_reader.h"

MODULE("PROC_CREATE", LOG_LEVEL_INFO);


// defined in assembly for switch / starting
extern void isr_body_exit_point();
extern void minimal_returning_function();


static pid_t   next_pid();

static error_t physical_pages_bulk_allocate(int num_pages, phys_addr_t **addresses_arr);
static error_t physical_pages_bulk_release(phys_addr_t *addresses_arr, int num_pages);
static error_t mem_region_allocate_and_map(mem_region_t *reg, page_dir_t page_dir);
static error_t mem_region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd);

static error_t clone_memory_region(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd);
static error_t clone_elf_regions(process_t *child, process_t *parent);
static error_t destroy_elf_regions(process_t *proc);
static error_t create_process_object(bool is_user_proc, page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr);


// ----------------------------------------------------------
typedef struct elf_page_load_info {
    virt_addr_t page_address;
    size_t file_offset;
    size_t page_offset;
    size_t load_size;
} elf_page_load_plan_t;
// ----------------------------------------------------------

static pid_t last_pid = 0;
static lock_t pid_lock = 0;

static pid_t next_pid() {
    mutex_acquire(&pid_lock);
    pid_t id = last_pid++;
    mutex_release(&pid_lock);
    return id;
}

// ----------------------------------------------------------
// building blocks

static error_t physical_pages_bulk_allocate(int num_pages, phys_addr_t **addresses_arr) {
    ASSERT(num_pages > 0);
    ASSERT(addresses_arr != NULL);
    log_trace("physical_pages_bulk_allocate(num=%d)", num_pages);

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
    log_trace("physical_pages_bulk_allocate() done");
    return OK;
}
static error_t physical_pages_bulk_release(phys_addr_t *addresses_arr, int num_pages) {
    ASSERT(addresses_arr != NULL);
    ASSERT(num_pages > 0);
    log_trace("physical_pages_bulk_release(addr=%p, num=%d)", addresses_arr, num_pages);

    for (int page = 0; page < num_pages; page++) {
        if (addresses_arr[page])
            pmm_free_physical_page(addresses_arr[page]);
    }

    kfree(addresses_arr);
    return OK;
}
static error_t mem_region_allocate_and_map(mem_region_t *reg, page_dir_t page_dir) {
    ASSERT(reg != NULL);
    ASSERT(page_dir > 0);
    log_trace("mem_region_allocate_and_map(reg=%p, page_dir=%x)", reg, page_dir);
    
    phys_addr_t *pages_arr;
    int num_pages = vmm_pages_for_size(reg->size);
    error_t err = physical_pages_bulk_allocate(num_pages, &pages_arr);
    if (err) return err;

    bool user = (reg->flags & REGION_USER_ACCESSIBLE) != 0;
    bool writable = (reg->flags & REGION_WRITE_ENABLE) != 0;
    // log_debug("mem_region_allocate_and_map, vaddr=0x%x, user=%d, write=%d", reg->address, (int)user, (int)writable);
    for (int page = 0; page < num_pages; page++) {
        err = vmm_map_page_to_other_pd(reg->address + page * vmm_page_size(), pages_arr[page], user, writable, page_dir);
        if (err) {
            log_warn("error while mapping page %d/%d, will unmap all and release physical pages", page, num_pages);
            while (--page >= 0) vmm_unmap_page_from_other_pd(reg->address + page * vmm_page_size(), page_dir);
            physical_pages_bulk_release(pages_arr, num_pages);
            return err;
        }
    }
    
    kfree(pages_arr);
    return OK;
}
static error_t mem_region_unmap_and_release(mem_region_t *reg, page_dir_t page_dir) {
    ASSERT(reg != NULL);
    ASSERT(page_dir > 0);
    ASSERT(vmm_is_page_aligned(reg->address));
    ASSERT(vmm_is_page_aligned(reg->size));
    log_trace("mem_region_unmap_and_release(reg=%p, page_dir=%x)", reg, page_dir);

    int num_pages = reg->size / vmm_page_size();
    virt_addr_t vaddr = reg->address;
    for (int page = 0; page < num_pages; page++) {
        phys_addr_t paddr = vmm_resolve(vaddr, page_dir);

        vmm_unmap_page_from_other_pd(vaddr, page_dir);
        if (paddr != 0)
            pmm_free_physical_page(paddr);
        
        vaddr += vmm_page_size();
    }

    *reg = mem_region_empty();
    return OK;
}
static error_t mem_region_copy_contents(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd) {
    log_trace("mem_region_copy_contents(dest_reg=%p, dest_pd=%x, src_reg=%p, src_pd=%x)", dest_reg, dest_pd, src_reg, src_pd);
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

// ----------------------------------------------------------------

static error_t allocate_and_map_elf_segment(process_t *proc, int section_no, elf_loadable_segment_t *seg) {
    ASSERT(proc != NULL);
    ASSERT(section_no < MAX_PROCESS_ELF_SECTIONS);
    ASSERT(seg != NULL);
    log_trace("allocate_and_map_elf_segment(proc=%p, section=%d, seg=%p)", proc, section_no, seg);

    virt_addr_t addr = vmm_round_down(seg->address_in_mem);
    size_t size = vmm_round_up(seg->address_in_mem + seg->size_in_mem) - addr;
    region_flags_t flags = REGION_USAGE_ELF |
        (seg->writable ? REGION_WRITE_ENABLE : REGION_READ_ONLY) |
        (proc_is_user_proc(proc) ? REGION_USER_ACCESSIBLE : REGION_SUPERVISOR_ONLY);
    
    mem_region_t reg = mem_region_of(addr, size, flags);
    error_t err = mem_region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;

    proc->memory.elf_sections[section_no] = reg;
    return OK;
}
static error_t prepare_elf_segment_loading_plan(process_t *proc, elf_loadable_segment_t *seg, elf_page_load_plan_t **array, int *num_pages) {
    virt_addr_t lowest_address = vmm_round_down(seg->address_in_mem);
    virt_addr_t highest_address = vmm_round_up(seg->address_in_mem + seg->size_in_mem);
    int pages_needed = (highest_address - lowest_address) / vmm_page_size();

    elf_page_load_plan_t *arr = kmalloc(sizeof(elf_page_load_plan_t) * pages_needed);

    virt_addr_t page_addr = lowest_address;
    size_t file_offset = seg->offset_in_file;
    size_t file_remaining = seg->size_in_file;
    size_t initial_gap = seg->address_in_mem - lowest_address;
    for (int page_no = 0; page_no < pages_needed; page_no++) {

        size_t available_mem = vmm_page_size() - initial_gap;
        size_t load_size = min(file_remaining, available_mem);

        arr[page_no].page_address = page_addr;
        arr[page_no].page_offset = initial_gap;
        arr[page_no].file_offset = file_offset;
        arr[page_no].load_size = load_size;

        page_addr += vmm_page_size();
        file_offset += load_size;
        file_remaining -= load_size;
        initial_gap = 0;
    }

    ASSERT(file_remaining == 0); // we managed to load all the segment from file

    *array = arr;
    *num_pages = pages_needed;
    return OK;
}
static error_t load_elf_segment_page(process_t *proc, open_file_t *elf, elf_page_load_plan_t *plan, char *page_buffer) {
    error_t err;
    size_t offset = 0;
    log_trace("load_elf_segment_page_per_plan(page=%p, file_offset=%u, mem_offset=%u, load_size=%u)", plan->page_address, plan->file_offset, plan->page_offset, plan->load_size);

    if (plan->page_offset > 0) {
        memset(page_buffer, 0, plan->page_offset);
        offset += plan->page_offset;
    }

    if (plan->load_size > 0) {
        vfs_seek(elf, plan->file_offset, SEEK_SET);
        err = vfs_read(elf, page_buffer + offset, plan->load_size);
        if (err < 0) return err;
        if ((size_t)err < plan->load_size) return ERR_READING_FILE;
        offset += plan->load_size;
    }

    if (offset < vmm_page_size())
        memset(page_buffer + offset, 0, vmm_page_size() - offset);

    return OK;
}
static error_t allocate_and_load_elf_segment_from_file(process_t *proc, open_file_t *elf, elf_loadable_segment_t *seg, char *page_buffer) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    ASSERT(seg != NULL);
    log_trace("allocate_and_load_elf_segment_from_file(proc=%p, elf=%p, seg=%p, buffer=%p)", proc, elf, seg, page_buffer);

    error_t err = OK;
    elf_page_load_plan_t *plans = NULL;
    int num_pages;

    log_debug("loading elf segment from file (0x%x/%u) into memory (0x%x/%u), flags=R%c%c",
        seg->offset_in_file, seg->size_in_file, seg->address_in_mem, seg->size_in_mem, seg->writable ? 'W' : '-', seg->executable ? 'X' : '-');

    int section_no = proc_used_elf_sections(proc);
    ASSERT(section_no >= 0 && section_no < MAX_PROCESS_ELF_SECTIONS);

    // this allocates and maps, to prepare the memory for loading
    err = allocate_and_map_elf_segment(proc, section_no, seg);
    if (err) goto exit;

    // now we need to take it page by page
    err = prepare_elf_segment_loading_plan(proc, seg, &plans, &num_pages);
    if (err) goto exit;
    for (int i = 0; i < num_pages; i++) {
        err = load_elf_segment_page(proc, elf, &plans[i], page_buffer);
        if (err) goto exit;

        // write the buffer onto the physical page
        phys_addr_t paddr = vmm_resolve(plans[i].page_address, proc->memory.page_dir);
        vmm_physpg_write(paddr, 0, page_buffer, vmm_page_size());
    }

exit:
    if (plans != NULL)
        kfree(plans);
    return traceable(err);
}
static error_t allocate_and_load_all_elf_segments_from_file(process_t *proc, open_file_t *elf, virt_addr_t *entry_point) {
    ASSERT(proc != NULL);
    ASSERT(elf != NULL);
    log_trace("allocate_and_load_all_elf_segments_from_file(proc=%p, elf=%p)", proc, elf);

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
        err = allocate_and_load_elf_segment_from_file(proc, elf, &segments_arr[i], page_buffer);
        if (err) goto exit;
    }
    
    err = elf_get_entry_point(elf, entry_point);
    if (err) goto exit;

    err = OK;

exit:
    if (page_buffer != NULL) kfree(page_buffer);
    if (segments_arr != NULL) kfree(segments_arr);
    return traceable(err);
}
static error_t clone_elf_regions(process_t *child, process_t *parent) {
    ASSERT(child != NULL);
    ASSERT(parent != NULL);
    log_trace("clone_elf_regions(child=%p, parent=%p)", child, parent);

    error_t err = OK;
    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        if (mem_region_is_empty(&parent->memory.elf_sections[i]))
            continue;
        
        err = clone_memory_region(&child->memory.elf_sections[i], child->memory.page_dir, &parent->memory.elf_sections[i], parent->memory.page_dir);
        if (err) return err;
    }

    return OK;
}

// ----------------------------------------------------------------

// in exec(), we need to prepare the future process user stack, 
// with env and arg before we destroy the old process heap.
// since pointers are absolute, we need to prepare them for their final virtual addresses
static error_t capture_argv_envp(char **argv, char **envp, uint32_t future_stack_top, char **page_buffer, uint32_t *future_esp) {
    log_trace("capture_argv_envp()");

    int strings_size = 0;
    int argc = 0;
    int envc = 0;
    for (int i = 0; argv != NULL && argv[i] != NULL; i++) {
        strings_size += strlen(argv[i]) + 1;
        argc++;
    }
    for (int i = 0; envp != NULL && envp[i] != NULL; i++) {
        strings_size += strlen(envp[i]) + 1;
        envc++;
    }
    uint32_t stack_used = round_up((4 + argc + 1 + envc + 1) * sizeof(uint32_t) + strings_size, 16);
    if (stack_used > vmm_page_size()) {
        log_error("cannot fit args and environment strings in a single page (argc=%d, envc=%d, strings_size=%d)", argc, envc, strings_size);
        return ERR_OVERFLOWN;
    }

    char *buff = kmalloc(vmm_page_size());
    memset(buff, 0, vmm_page_size());
    uint32_t *stack = (uint32_t *)(buff + vmm_page_size() - stack_used);
    char *strings_ptr = ((char *)stack) + ((4 + argc + 1 + envc + 1)) * sizeof(uint32_t);
    int si = 0;

    stack[si++] = 0; // return address, but _start() will never return anywhere
    stack[si++] = argc; 
    stack[si++] = future_stack_top - stack_used +  4             * sizeof(uint32_t); // argv pointer table address
    stack[si++] = future_stack_top - stack_used + (4 + argc + 1) * sizeof(uint32_t); // envp pointer table address

    // write both, the actual string table and the pointers to them, at the same time
    for (int i = 0; argv != NULL && argv[i] != NULL; i++) {
        stack[si++] = future_stack_top - stack_used + ((uint32_t)strings_ptr - (uint32_t)stack);
        strcpy(strings_ptr, argv[i]);
        strings_ptr += strlen(argv[i]) + 1;
    }
    stack[si++] = 0; // final null pointer for argv
    for (int i = 0; envp != NULL && envp[i] != NULL; i++) {
        stack[si++] = future_stack_top - stack_used + ((uint32_t)strings_ptr - (uint32_t)stack);
        strcpy(strings_ptr, envp[i]);
        strings_ptr += strlen(envp[i]) + 1;
    }
    stack[si++] = 0; // final null pointer for envp

    // log_debug("prepared user stack follows, should see:  return address, argc, argv, envp, tables, strings");
    // log_debug_hex(buff + vmm_page_size() - stack_used, stack_used, (uintptr_t)(future_stack_top - stack_used));
    
    *page_buffer = buff;
    *future_esp = future_stack_top - stack_used;
    return OK;
}

// ----------------------------------------------------------------

static error_t create_kernel_stack(process_t *proc, uint32_t user_entry_point, uint32_t user_stack_pointer, interrupt_frame_t *possible_parent_frame) {  // creates minimal ring0 stack to start a user process
    log_trace("create_kernel_stack(proc=%p, user_entry=%p, user_esp=%p, possible_parent_frame=%p)", proc, user_entry_point, user_stack_pointer, possible_parent_frame);

    proc->memory.ring0_stack.size = 4096;
    proc->memory.ring0_stack.address = (uintptr_t)kmalloc(proc->memory.ring0_stack.size);
    if (proc->memory.ring0_stack.address == 0)
        return ERR_NO_MEMORY;

    uint32_t ksp = proc->memory.ring0_stack.address + proc->memory.ring0_stack.size;
    ksp -= sizeof(interrupt_frame_t); interrupt_frame_t *iframe = (interrupt_frame_t *)ksp;
    ksp -= sizeof(uint32_t);          uint32_t *ret_address = (uint32_t *)ksp;
    ksp -= sizeof(c_frame_t);         c_frame_t *cframe = (c_frame_t *)ksp;

    proc->memory.ring0_esp = (uint32_t)cframe;
    proc->memory.ring0_stack_top = proc->memory.ring0_stack.address + proc->memory.ring0_stack.size;

    if (possible_parent_frame == NULL) {
        // this is a new frame, for spawn() or exec()
        uint32_t code_seg = proc_is_kernel_proc(proc) ? KERNEL_CODE_SEGMENT : USER_CODE_SEGMENT | RING3_RPL;
        uint32_t data_seg = proc_is_kernel_proc(proc) ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT | RING3_RPL;

        memset(iframe, 0, sizeof(interrupt_frame_t));
        iframe->eip      = user_entry_point;
        iframe->cs       = code_seg;
        iframe->user_esp = user_stack_pointer; // ignored in kernel tasks
        iframe->ss       = data_seg;
        iframe->eflags   = 0x202;  // interrupts enabled, this is important
        iframe->ds       = data_seg;
        iframe->es       = data_seg;
        iframe->fs       = data_seg;
        iframe->gs       = data_seg;

    } else {
        // this is a fork, return to same exact environment
        memcpy(iframe, possible_parent_frame, sizeof(interrupt_frame_t));
    }

    *ret_address = (uint32_t)isr_body_exit_point; // when miminal_returning_function returns, it returns here

    memset(cframe, 0, sizeof(c_frame_t));
    cframe->eip = (uint32_t)minimal_returning_function; // when switch_inside_c_function returns, it returns here

    log_trace("create_kernel_stack(), done");
    return OK;
}

static error_t destroy_kernel_stack(process_t *proc) {
    log_trace("destroy_kernel_stack(proc=%p)", proc);

    if (proc->memory.ring0_stack.address != 0) {
        kfree((void *)proc->memory.ring0_stack.address);
        proc->memory.ring0_stack.address = 0;
        proc->memory.ring0_stack.size = 0;
    }

    return OK;
}

static error_t create_user_stack(process_t *proc, uint32_t stack_top, char *top_page_buffer) {
    log_trace("create_user_stack(proc=%p)", proc);

    ASSERT(proc_is_user_proc(proc));
    ASSERT(proc->memory.page_dir != 0);

    virt_addr_t addr = stack_top;
    size_t size = 512 * KB;
    mem_region_t reg = mem_region_of(addr - size, size, REGION_USAGE_STACK | REGION_WRITE_ENABLE | REGION_USER_ACCESSIBLE);
    error_t err = mem_region_allocate_and_map(&reg, proc->memory.page_dir);
    if (err) return err;
    proc->memory.user_stack = reg;

    // transfer buffer to the physical page of the stack, not to the virtual one.
    uint32_t stack_top_page_virt_addr = proc->memory.user_stack.address + proc->memory.user_stack.size - vmm_page_size();
    uint32_t stack_top_page_phys_addr = vmm_resolve(stack_top_page_virt_addr, proc->memory.page_dir);
    vmm_physpg_write(stack_top_page_phys_addr, 0, top_page_buffer, vmm_page_size());
    
    kfree(top_page_buffer);
    return OK;
}

static error_t clone_user_stack(process_t *child, process_t *parent) {
    log_trace("clone_user_stack(child=%p, parent=%p)", child, parent);

    ASSERT(child != NULL && parent != NULL && child->memory.page_dir != 0);
    ASSERT(proc_is_user_proc(child));
    error_t err;

    mem_region_t reg = parent->memory.user_stack; // copy values
    err = mem_region_allocate_and_map(&reg, child->memory.page_dir);
    if (err) return err;

    err = mem_region_copy_contents(&reg, child->memory.page_dir, &parent->memory.user_stack, parent->memory.page_dir);
    if (err) return err;

    child->memory.user_stack = reg;
    return OK;
}

static error_t destroy_user_stack(process_t *proc) {
    log_trace("destroy_user_stack(proc=%p)", proc);

    if (proc && proc->memory.user_stack.address != 0) {
        mem_region_unmap_and_release(&proc->memory.user_stack, proc->memory.page_dir);
        proc->memory.user_stack.address = 0;
        proc->memory.user_stack.size = 0;
    }

    return OK;
}

static error_t create_user_heap(process_t *proc) {
    log_trace("create_user_heap(proc=%p)", proc);

    // ensure above all elf sections
    virt_addr_t addr = 512 * MB;  // 0x20000000
    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        mem_region_t *r = &proc->memory.elf_sections[i];
        addr = max(addr, vmm_round_up(r->address + r->size));
    }
    size_t size = 64 * KB;

    mem_region_t region = mem_region_of(addr, size, REGION_USAGE_HEAP | REGION_WRITE_ENABLE | REGION_USER_ACCESSIBLE);
    error_t err = mem_region_allocate_and_map(&region, proc->memory.page_dir);
    if (err) return err;
    
    proc->memory.user_heap = region;
    return OK;
}

static error_t clone_user_heap(process_t *child, process_t *parent) {
    log_trace("clone_user_heap(child=%p, parent=%p)", child, parent);

    error_t err;

    mem_region_t region = parent->memory.user_heap;

    err = mem_region_allocate_and_map(&region, child->memory.page_dir);
    if (err) return err;
    err = mem_region_copy_contents(&region, child->memory.page_dir, &parent->memory.user_heap, parent->memory.page_dir);
    if (err) return err;
    
    child->memory.user_heap = region;
    return OK;
}

static error_t destroy_user_heap(process_t *proc) {
    log_trace("destroy_user_heap(proc=%p)", proc);

    if (proc && proc->memory.user_heap.address != 0) {
        mem_region_unmap_and_release(&proc->memory.user_heap, proc->memory.page_dir);
        proc->memory.user_heap.address = 0;
        proc->memory.user_heap.size = 0;
    }

    return OK;
}

static error_t destroy_elf_regions(process_t *proc) {
    log_trace("destroy_elf_regions(proc=%p)", proc);
    ASSERT(proc != NULL);

    for (int i = 0; i < MAX_PROCESS_ELF_SECTIONS; i++) {
        mem_region_t *reg = &proc->memory.elf_sections[i];
        if (mem_region_is_empty(reg))
            continue;

        mem_region_unmap_and_release(reg, proc->memory.page_dir);
    }
    
    return OK;
}

static error_t clone_memory_region(mem_region_t *dest_reg, page_dir_t dest_pd, mem_region_t *src_reg, page_dir_t src_pd) {
    ASSERT(dest_reg != NULL);
    ASSERT(dest_pd != 0);
    log_trace("clone_memory_region(dest_reg=%p, dest_pd=%x, src_reg=%p, src_pd=%x)", dest_reg, dest_pd, src_reg, src_pd);

    error_t err = OK;
    bool region_allocated = false;

    *dest_reg = *src_reg; // copy values

    err = mem_region_allocate_and_map(dest_reg, dest_pd);
    if (err) goto error;
    region_allocated = true;

    err = mem_region_copy_contents(dest_reg, dest_pd, src_reg, src_pd);
    if (err) goto error;

    return OK;
error:
    if (region_allocated)
        mem_region_unmap_and_release(dest_reg, dest_pd);
    *dest_reg = mem_region_empty(); // clear
    return traceable(err);
}

static error_t init_filesystem_stuff(process_t *proc) {
    log_trace("init_filesystem_stuff(proc=%d)", proc);
    error_t err;

    err = proc_chroot(proc, "/");
    if (err) return err;
    err = proc_chdir(proc, "/");
    if (err) return err;
    
    err = proc_open(proc, "/dev/tty0", 0);
    if (err < 0) return err;

    proc_dup2(proc, 0, proc, 1);
    proc_dup2(proc, 0, proc, 2);

    return OK;
}    

static error_t inherit_filesystem_stuff(process_t *child, process_t *parent) {
    log_trace("inherit_filesystem_stuff(child=%d, parent=%d)", child, parent);

    ASSERT(parent != NULL);

    memcpy(&child->vfs_ctx, &parent->vfs_ctx, sizeof(vfs_context_t));
    child->cwd_path = strdup(parent->cwd_path);
    
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (parent->file_handles[i] == NULL)
            continue;
        proc_dup2(parent, i, child, i);
    }

    return OK;
}

static error_t destroy_filesystem_stuff(process_t *proc) {

    if (proc->cwd_path) {
        kfree(proc->cwd_path);
        proc->cwd_path = NULL;
    }

    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (proc->file_handles[i] == NULL)
            continue;
        
        // just release, others may have opened same handles
        open_files.release(proc->file_handles[i]);
    }

    return OK;
}

// ----------------------------------------------------------------

static error_t create_process_object(bool is_user_proc, page_dir_t pd, process_t *parent, proc_priority_t priority, const char *name, process_t **proc_ptr) {
    ASSERT(pd != 0);
    ASSERT(name != NULL);
    ASSERT(proc_ptr != NULL);

    log_trace("create_process_object()");

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

    *proc_ptr = proc;
    return OK;

failed:
    if (proc && proc->name) kfree(proc->name);
    if (proc && proc->memory.ring0_stack.address) kfree((void *)proc->memory.ring0_stack.address);
    if (proc) kfree(proc);
    return traceable(err);
}

error_t process_create_for_kernel(const char *name, uintptr_t function_to_call, proc_priority_t priority, process_t **proc_ptr) {
    log_trace("process_create_for_kernel(name=%s)", name);

    process_t *proc;
    error_t err = create_process_object(false, vmm_get_kernel_page_directory(), NULL, priority, name, &proc);
    if (err) return err;

    err = create_kernel_stack(proc, function_to_call, 0, NULL);
    if (err) return err;

    ASSERT(proc->memory.ring0_stack.address != 0);
    ASSERT(proc->memory.ring0_stack.size != 0);
    ASSERT(proc->memory.ring0_esp != 0);
    ASSERT(proc->memory.ring0_stack_top != 0);

    // log_debug_fmt(proc_log_formatter, "process_create_for_kernel(): ", proc);
    *proc_ptr = proc;
    return OK;
}

error_t process_create_for_spawn(process_t *parent, const char *file_path, char **argv, char **envp, proc_priority_t priority, process_t **proc_ptr) {
    ASSERT(file_path != 0); 
    ASSERT(proc_ptr != 0);
    log_trace("process_v2_create_for_spawn(parent=%p, file='%s')", parent, file_path);

    error_t err = OK;
    open_file_t *elf = NULL;
    process_t *child = NULL;
    page_dir_t pd = 0;
    char *page_buffer = NULL; // initialize before error handling.

    pd = vmm_create_user_page_directory();
    if (pd == 0) { err = ERR_NO_MEMORY; goto failed; }
    
    err = create_process_object(true, pd, parent, priority, file_path, &child);
    if (err) goto failed;
    pd = 0; // from now on, proc_destroy() shall destroy the PD, not us
    
    // prepare filesystem in order to be able to open the executable
    if (parent == NULL) err = init_filesystem_stuff(child);
    else err = inherit_filesystem_stuff(child, parent);
    if (err) goto failed;

    err = vfs_open(&child->vfs_ctx, file_path, 0, &elf);
    if (err) goto failed;
    err = elf_verify_executable(elf); // verify early for better recovery
    if (err) goto failed;

    virt_addr_t user_exec_address = 0;
    err = allocate_and_load_all_elf_segments_from_file(child, elf, &user_exec_address);
    if (err) goto failed;
    
    err = create_user_heap(child);
    if (err) goto failed;

    uint32_t user_stack_pointer;
    err = capture_argv_envp(argv, envp, 2 * GB, &page_buffer, &user_stack_pointer);
    if (err) goto failed;

    err = create_user_stack(child, 2 * GB, page_buffer);
    if (err) goto failed;

    err = create_kernel_stack(child, user_exec_address, user_stack_pointer, NULL);
    if (err) goto failed;

    vfs_close(elf);
    
    *proc_ptr = child;
    return OK;

failed:
    if (pd) vmm_destroy_user_page_directory(pd);
    if (elf) vfs_close(elf);
    if (child) proc_destroy(child);
    return traceable(err);
}

error_t process_replace_for_exec(process_t *proc, const char *file_path, char **argv, char **envp) {
    ASSERT(proc != NULL);
    ASSERT(file_path != NULL);
    log_trace("process_replace_for_exec(proc=%p, file='%s')", proc, file_path);

    error_t err = OK;
    open_file_t *elf = NULL;
    char *page_buffer = NULL;  // initialize before error handling next time!!!!

    err = vfs_open(&proc->vfs_ctx, file_path, 0, &elf);
    if (err) goto failed;
    err = elf_verify_executable(elf); // verify early for better recovery
    if (err) goto failed;

    // capture arguments passed in, before destroying the heap.
    uint32_t user_stack_pointer;
    err = capture_argv_envp(argv, envp, 2 * GB, &page_buffer, &user_stack_pointer);
    if (err) goto failed;

    // improve this:
    if (proc->name) kfree(proc->name);
    proc->name = kstrdup(file_path);

    // now we can release memory regions (or create new process?)
    err = destroy_user_heap(proc);
    if (err) goto failed;

    err = destroy_user_stack(proc);
    if (err) goto failed;
    
    err = destroy_elf_regions(proc);
    if (err) goto failed;

    // then initialize new ones all over again, as for spawn()
    log_debug_fmt(proc_log_formatter, "after destroying memory regions:", proc);

    virt_addr_t user_exec_address;
    err = allocate_and_load_all_elf_segments_from_file(proc, elf, &user_exec_address);
    if (err) goto failed;

    vfs_close(elf);
    elf = NULL;

    err = create_user_heap(proc);
    if (err) goto failed;

    err = create_user_stack(proc, 2 * GB, page_buffer);
    if (err) goto failed;

    // we maintain the same kernel stack (same esp0), but need to change the user addresses
    interrupt_frame_t *iframe = proc_get_interrupt_frame(proc);
    iframe->user_esp = user_stack_pointer;
    iframe->eip = user_exec_address;

    // Current working directory and open files are preserved.
    
    return OK;

failed:
    if (elf) vfs_close(elf);
    if (page_buffer) kfree(page_buffer);
    // we may be in a pretty unrunnable state! 
    // real kernels solve this by loading new image and if success, swap. not blindly destroying first
    return traceable(err);
}

error_t process_create_for_fork(process_t *parent, process_t **proc_ptr) {
    ASSERT(parent != NULL);
    ASSERT(proc_ptr != NULL);
    log_trace("process_create_for_fork(parent=%p)", parent);

    error_t err = OK;
    process_t *child = NULL;
    page_dir_t pd = 0;

    pd = vmm_create_user_page_directory();
    
    err = create_process_object(true, pd, parent, parent->priority, parent->name, &child);
    if (err) goto failed;
    pd = 0; // from now on, the process shall destroy the PD

    uint32_t parent_return_address = proc_get_interrupt_frame(parent)->eip;
    err = clone_elf_regions(child, parent);
    if (err) goto failed;

    err = clone_user_heap(child, parent);
    if (err) goto failed;
    
    uint32_t user_stack_pointer = proc_get_interrupt_frame(parent)->user_esp;
    err = clone_user_stack(child, parent);
    if (err) goto failed;

    err = create_kernel_stack(child, parent_return_address, user_stack_pointer, proc_get_interrupt_frame(parent));
    if (err) goto failed;

    err = inherit_filesystem_stuff(child, parent);
    if (err) goto failed;

    // make fork() return 0 to the child
    proc_get_interrupt_frame(child)->eax = 0;

    *proc_ptr = child;
    return OK;

failed:
    if (pd) vmm_destroy_user_page_directory(pd);
    if (child) proc_destroy(child);
    return traceable(err);
}

// after a process has terminated, clean up resources
void proc_destroy(process_t *proc) {
    
    if (proc->parent)
        proc_remove_child(proc->parent, proc);

    if (proc->name != NULL)
        kfree(proc->name);

    destroy_elf_regions(proc);
    destroy_user_heap(proc);
    destroy_user_stack(proc);
    destroy_kernel_stack(proc);
    destroy_filesystem_stuff(proc);

    if (proc->memory.page_dir != 0 && proc->memory.page_dir != vmm_get_kernel_page_directory())
        vmm_destroy_user_page_directory(proc->memory.page_dir);
    
    // can't think (atm) of anything else to free
    if (proc->name)
        kfree(proc->name);
    kfree(proc);
}
