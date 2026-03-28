#include "../include/ctypes.h"
#include "../include/macros.h"
#include "../utils/assert.h"
#include "../include/bits.h"
#include "../arch/gdt.h"
#include "physmem.h"
#include "vmm.h"
#include "kmemmap.h"
#include "../utils/panic.h"
#include "../utils/mutex.h"
#include "../memory/kheap.h"
#include "../logger/logger.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../memory/mem_region.h"

#include "vmm_internal.h"

struct kinfo kinfo;





static phys_addr_t vmm_allocate_kernel_mapping_page() {
    mutex_acquire(&kinfo.work_pages_lock);
    if (kinfo.mapping_pages_allocated >= kinfo.mapping_pages_count)
        panic("Cannot allocate any more mapping pages, exhausted all %d of them", kinfo.mapping_pages_count);
    
    phys_addr_t page = kinfo.mapping_pages_addr + kinfo.mapping_pages_allocated * vmm_page_size();
    kinfo.mapping_pages_allocated++;

    mutex_release(&kinfo.work_pages_lock);
    return page;
}

void vmm_initialize(phys_addr_t kernel_reserved_area_start, phys_addr_t kernel_reserved_area_end, phys_addr_t utility_pages_addr, size_t utility_pages_size) {
    log_trace("vmm_initialize(kstart=0x%x, kend=0x%x, uaddr=0x%x, usize=%d)", kernel_reserved_area_start, kernel_reserved_area_end, utility_pages_addr, utility_pages_size);

    ASSERT(kernel_reserved_area_end > kernel_reserved_area_start);
    ASSERT(utility_pages_addr > 0);
    ASSERT(vmm_is_page_aligned(utility_pages_size));
    ASSERT(vmm_round_down(utility_pages_size) / vmm_page_size() >= 4);

    memset(&kinfo, 0, sizeof(kinfo));

    kinfo.reserved_area_start = kernel_reserved_area_start;
    kinfo.reserved_area_end = kernel_reserved_area_end;
    kinfo.mapping_pages_addr = utility_pages_addr;
    kinfo.mapping_pages_count = utility_pages_size / vmm_page_size();;

    kinfo.page_directory = vmm_allocate_kernel_mapping_page();
    kinfo.work_page1_addr = vmm_allocate_kernel_mapping_page();
    kinfo.work_page2_addr = vmm_allocate_kernel_mapping_page();
    log_debug("kernel page_dir=0x%x, work_page1=0x%x, work_page2=0x%x", kinfo.page_directory, kinfo.work_page1_addr, kinfo.work_page2_addr);

    // create a page directory for kernel.
    vmm_create_kernel_page_directory_using_mapping_pages(kinfo.page_directory, kinfo.reserved_area_start, kinfo.reserved_area_end);
    log_debug("after creating kernel page directory, %d mapping pages allocated of %d total", kinfo.mapping_pages_allocated, kinfo.mapping_pages_count);
    
    // log_debug_hex((void *)kinfo.page_directory, 16 * 4, 0);
    log_info_fmt(vmm_pagedir_log_formatter, "kmap:", kinfo.page_directory);

    // now enable paging (fingers crossed!)
    vmm_set_page_directory_register(kinfo.page_directory);
    vmm_enable_paging();
}

static void vmm_create_kernel_page_directory_using_mapping_pages(page_dir_t kernel_pd, virt_addr_t start_addr, virt_addr_t end_addr) {
    // Identity map the kernel before paging is enabled.
    ASSERT(kernel_pd != 0);
    ASSERT(vmm_is_page_aligned(start_addr));
    ASSERT(vmm_is_page_aligned(end_addr));

    memset((void *)kernel_pd, 0, vmm_page_size());
    // map page directory to last 4MB of physical memory, so it can be accessed anytime (RECURSIVE_MAPPING_BASE_ADDRESS)
    _set_table_entry(kernel_pd, 1023, pd_entry_of(kernel_pd, false, false, true));

    for (virt_addr_t addr = start_addr; addr < end_addr; addr += 4096) {
        // Determine page directory index
        int pd_index = (int)(addr >> 22); // bits 31-22
        int pt_index = (int)((addr >> 12) & 0x3FF); // bits 21-12

        // Allocate page table if not already present
        uint32_t pd_entry = ((uint32_t *)kernel_pd)[pd_index];
        virt_addr_t page_table;
        if (entry_is_present(pd_entry)) {
            page_table = entry_get_address(pd_entry);
        } else {
            page_table = vmm_allocate_kernel_mapping_page();
            if (!page_table) panic("Cannot allocate kernel page table");
            memset((void *)page_table, 0, vmm_page_size());

            uint32_t entry = pd_entry_of(
                page_table,
                false, // user access
                true,  // write enable
                true   // present
            );
            ((uint32_t *)kernel_pd)[pd_index] = entry;
        }

        // Set the page table entry
        uint32_t pt_entry = pt_entry_of(
            addr,  // physical address
            false, // user access
            true,  // writable
            true   // present
        );
        ((uint32_t *)page_table)[pt_index] = pt_entry;
    }
}

virt_addr_t vmm_get_kernel_area_end() {
    ASSERT(kinfo.reserved_area_end != 0);
    return kinfo.reserved_area_end;
}

phys_addr_t vmm_resolve(virt_addr_t virtual_addr, page_dir_t page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)
    int index;
    uint32_t entry;
    phys_addr_t address;

    // first resolve page directory.
    index = page_dir_index(virtual_addr);
    entry = vmm_physpg_get_entry(page_dir_addr, index);
    if (!entry_is_present(entry))
        return 0;
    address = entry_get_address(entry);
    if (address == 0)
        return 0;
    
    // then resolve page table
    index = page_table_index(virtual_addr);
    entry = vmm_physpg_get_entry(address, index);
    if (!entry_is_present(entry))
        return 0;
    address = entry_get_address(entry);
    if (address == 0)
        return 0;
    
    // now resolve final address
    uint32_t offset = _virt_addr_to_physical_page_offset(virtual_addr);
    return (address + offset);
}

void _map_page_primitive(virt_addr_t virtual_addr, phys_addr_t physical_addr) {
    // log_trace("_map_page_primitive(virt=0x%x, phys=0x%x)", virtual_addr, physical_addr);

    // this function is expected operate on the work pages the kernel has.
    // it will not setup an new PTE, therefore it will not recurse.
    // it also modifies PT entries of the current PD, therefore expects RMW to work
    // it uses the utility pages, to map various physical addresses
    
    ASSERT(kinfo.paging_enabled);
    ASSERT(vmm_is_page_aligned(virtual_addr));
    ASSERT(vmm_is_page_aligned(physical_addr));
    ASSERT(virtual_addr == kinfo.work_page1_addr || virtual_addr == kinfo.work_page2_addr);

    // entry for page table MUST be there (setup by kernel)
    int pd_index = page_dir_index(virtual_addr);
    uint32_t entry = rmw_get_pd_entry(pd_index);
    ASSERT(entry_is_present(entry));

    // now, we may or may not have a value there.
    // no matter what was there, we will just rewrite it.
    int pt_index = page_table_index(virtual_addr);
    entry = pt_entry_of(physical_addr, false, true, true);
    rmw_set_pt_entry(pd_index, pt_index, entry);
    
    // invalidate for CPU to recalculate
    vmm_invalidate_cached_address(virtual_addr);
}

void _unmap_page_primitive(virt_addr_t virtual_addr) {
    // log_trace("_unmap_page_primitive(virt=0x%x)", virtual_addr);

    ASSERT(kinfo.paging_enabled);
    ASSERT(vmm_is_page_aligned(virtual_addr));
    ASSERT(virtual_addr == kinfo.work_page1_addr || virtual_addr == kinfo.work_page2_addr);

    // entry for page table MUST be there (setup by kernel)
    int pd_index = page_dir_index(virtual_addr);
    uint32_t entry = rmw_get_pd_entry(pd_index);
    ASSERT(entry_is_present(entry));

    int pt_index = page_table_index(virtual_addr);
    entry = rmw_get_pt_entry(pd_index, pt_index);
    ASSERT(entry_is_present(entry));

    // now clear it
    rmw_set_pt_entry(pd_index, pt_index, 0);
    
    // invalidate for CPU to recalculate
    vmm_invalidate_cached_address(virtual_addr);
}

error_t vmm_map_page_to_current_pd(virt_addr_t virtual_addr, virt_addr_t physical_addr, bool user_accessible, bool write_enable) {
    return rmw_map_page(virtual_addr, physical_addr, user_accessible, write_enable);
}

void vmm_unmap_page_from_current_pd(virt_addr_t virtual_addr) {
    rmw_unmap_page(virtual_addr);
}

error_t vmm_map_page_to_other_pd(virt_addr_t virtual_addr, virt_addr_t physical_addr, bool user_accessible, bool write_enable, page_dir_t page_dir) {
    log_trace("vmm_map_page_to_other_pd(virt=0x%x, phys=0x%x, page_dir=0x%x)", virtual_addr, physical_addr, page_dir);

    int index;
    uint32_t entry;

    // from the page directory, find or create the page table
    index = page_dir_index(virtual_addr);
    entry = vmm_physpg_get_entry(page_dir, index);

    virt_addr_t page_table_paddr;
    if (entry_is_present(entry)) {
        page_table_paddr = entry_get_address(entry);
    } else {
        // we need a new page for a page table. we need to acquire 
        // using different means between user space and kernel.
        if (page_dir == vmm_get_kernel_page_directory()) {
            page_table_paddr = vmm_allocate_kernel_mapping_page();
        } else {
            page_table_paddr = pmm_allocate_physical_page();
        }
        if (page_table_paddr == 0)
            return ERR_NO_MEMORY;
        
        // map/clear/unmap to initialize the PT
        vmm_physpg_clear(page_table_paddr);
        
        // map/update/unmap, to add the new PT in the PD
        uint32_t page_dir_value = pd_entry_of(page_table_paddr, user_accessible, true, true);
        vmm_physpg_set_entry(page_dir, index, page_dir_value);
    }

    // map/update/unmap to set the entry in the PT
    index = page_table_index(virtual_addr);
    entry = pt_entry_of(physical_addr, user_accessible, write_enable, true);
    vmm_physpg_set_entry(page_table_paddr, index, entry);
    
    return OK;
}

void vmm_unmap_page_from_other_pd(virt_addr_t virtual_addr, page_dir_t page_dir) {
    log_trace("vmm_unmap_page_from_pd(virt=0x%x, page_dir=0x%x)", virtual_addr, page_dir);

    // from the page directory, find or create the page table
    int index = page_dir_index(virtual_addr);
    uint32_t entry = ((uint32_t *)page_dir)[index];
    if (!entry_is_present(entry))
        return;

    // map/update/unmap to clear the entry in the PT
    phys_addr_t page_table_paddr = entry_get_address(entry);
    _map_page_primitive(vmm_workpg1(), page_table_paddr);
    index = page_table_index(virtual_addr);
    ((uint32_t *)vmm_workpg1())[index] = 0;
    vmm_workpg1_unmap();
}


// map a range to itself
error_t vmm_identity_map_range(virt_addr_t start_addr, virt_addr_t end_addr, page_dir_t page_dir_addr) {
    log_trace("vmm_identity_map_range(start=0x%x, end=0x%x, page_dir=0x%x)", start_addr, end_addr, page_dir_addr);

    for (virt_addr_t addr = start_addr; addr < end_addr; addr += vmm_page_size()) {
        error_t err = vmm_map_page_to_other_pd(addr, addr, false, true, page_dir_addr);
        if (err) return err;
    }

    return OK;
}


// Enabling paging is actually very simple. All that is needed is 
// to load CR3 with the address of the page directory 
// and to set the paging (PG) and protection (PE) bits of CR0.
void vmm_set_page_directory_register(page_dir_t value) {
    log_trace("vmm_set_page_directory_register(0x%x)", value);

    __asm__ __volatile__(
        "mov %0, %%eax\n\t"
        "mov %%eax, %%cr3\n\t"
        :             // no output values
        : "g"(value)  // input No 0
        : "eax"       // garbled registers
    );
}

// get current directory register (cr3)
page_dir_t vmm_get_current_page_dir() {
    page_dir_t value;

    __asm__ __volatile__(
        "movl %%cr3, %0\n\t"
        : "=g"(value)  // output No 0
        :              // no input values
        :              // garbled registers
    );

    return value;
}

inline void vmm_invalidate_cached_address(virt_addr_t virtual_addr) {
    // supported on i486+
    asm volatile("invlpg (%0)" : : "r" (virtual_addr) : "memory");
}

void vmm_enable_paging() {
    log_trace("Enabling memory paging in CPU");

    // enable big pages support
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);        // CR4.PSE
    asm volatile("mov %0, %%cr4" :: "r"(cr4));    


    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"  // turn on bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );

    kinfo.paging_enabled = true;
}

void vmm_disable_paging() {
    log_trace("Disabling memory paging in CPU");

    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "and $0x7FFFFFFF, %%eax\n\t"  // turn off bit 31
        "mov %%eax, %%cr0"
        :       // no outputs
        :       // no inputs
        : "eax" // garbled registers
    );
}


page_dir_t vmm_get_kernel_page_directory() {
    return kinfo.page_directory;
}

#define MAX_PAGE_FAULTS  5
static int page_fault_num = 0;

// handles page faults. 
// see https://wiki.osdev.org/Exceptions#Page_Fault
void vmm_page_fault_handler(trap_frame_t *tf) {

    page_fault_num++;
    if (page_fault_num > MAX_PAGE_FAULTS)
        panic("Too many page faults");

    uint32_t error_code = tf->err_code;
    uint32_t eip = tf->eip;

    uint32_t addr = 0;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(addr));

    page_dir_t cr3 = 0;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    char *mem_area = "";
    if      (mem_region_contains_address(&kmm.code, addr))          mem_area = "kernel code";
    else if (mem_region_contains_address(&kmm.data, addr))          mem_area = "kernel data";
    else if (mem_region_contains_address(&kmm.rodata, addr))        mem_area = "kernel rodata";
    else if (mem_region_contains_address(&kmm.bss, addr))           mem_area = "kernel bss";
    else if (mem_region_contains_address(&kmm.stack, addr))         mem_area = "kernel stack";
    else if (mem_region_contains_address(&kmm.mapping_pages, addr)) mem_area = "kernel mapping pages";
    else if (mem_region_contains_address(&kmm.pmm_bitmap, addr))    mem_area = "kernel pmm bmp";
    else if (mem_region_contains_address(&kmm.heap, addr))          mem_area = "kernel heap";
    else if (addr >= kmm.reserved_start && addr < kmm.reserved_end) mem_area = "kernel reserved area";
    else if (addr >= kmm.reserved_end)                              mem_area = "user memory space";

    log_warn("Page Fault (#%d):", page_fault_num);
    log_warn("   CS 0x%08x (0x%x is kernel, 0x%x is user)", tf->cs, KERNEL_CODE_SEGMENT, USER_CODE_SEGMENT);
    log_warn("  EIP 0x%08x (addr2line -f -e ./kernel/kernel.elf 0x%x)", eip, eip);
    log_warn("  CR3 0x%08x (0x%x is kernel)", cr3, kinfo.page_directory);
    log_warn("  CR2 0x%08x address space: %s", addr, mem_area);
    log_warn("  err 0x%08x", error_code);
    log_warn("      - P: %s", IS_BIT(error_code, 0) ? "page protection" : "page missing");
    log_warn("      - W: %s", IS_BIT(error_code, 1) ? "write attempt" : "read attempt");
    log_warn("      - U: %s", IS_BIT(error_code, 1) ? "ring 3 (user)" : "ring 0-2 (supervisor)");

    // log_debug_fmt(vmm_pagedir_log_formatter, "cr3:", cr3);
    // vmm_read_all_identity_addresses();

    // solution for now is to identity map this, just for fun
    // but, if we had a memory map (the mem_regions), we could identify who errored
    // e.g. stack underflow, or heap overflow, guard, mem-mapped file, etc
    if (addr >= kmm.reserved_start && addr < kmm.reserved_end) {
        log_warn("this is kernel space, will attempt identity mapping, but needs fixing");
        error_t err = rmw_map_page(vmm_round_down(addr), vmm_round_down(addr), false, true);
        if (err)
            panic("Failed to identity map: %d - %s", err, strerror(err));
    }
}

// allocates and creates a new page directory
page_dir_t vmm_create_page_directory(bool map_kernel_space) {
    ASSERT(kinfo.paging_enabled); // we rely on this

    page_dir_t page_dir = pmm_allocate_physical_page();
    if (page_dir == 0)
        return 0;

    log_trace("vmm_create_page_directory(), new PD 0x%x", page_dir);
    vmm_physpg_clear(page_dir);

    if (map_kernel_space) {
        log_debug("copying kernel PDE entries to new page_directory");
        vmm_physpg_copy(page_dir, kinfo.page_directory);
    }

    // map page directory to last 4MB of physical memory, so it can be accessed anytime
    vmm_physpg_set_entry(page_dir, 1023, pd_entry_of(page_dir, true, false, true));

    return page_dir;
}

// allocates pages and maps them to the virtual addresses requested (end address is non-inclusive)
error_t vmm_allocate_memory_range_this_pd(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end) {
    log_trace("vmm_allocate_memory_range_this_pd(0x%p - 0x%p)", virt_addr_start, virt_addr_end);

    // this is called by sbrk(), so user process
    for (virt_addr_t virt_addr = virt_addr_start; virt_addr < virt_addr_end; virt_addr += 4096) {
        virt_addr_t phys_page_addr = pmm_allocate_physical_page();
        if (phys_page_addr == 0)
            return ERR_NO_MEMORY;
        
        error_t err = rmw_map_page(virt_addr, phys_page_addr, true, true);
        if (err) {
            pmm_free_physical_page(phys_page_addr);
            return err;
        }
    }

    return OK;
}

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_page_directory(page_dir_t page_dir_address) {
    log_trace("vmm_destroy_page_directory(0x%x)", page_dir_address);
    pushcli();

    // free linked tables and pages 
    uint32_t entry;
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        entry = _get_table_entry(page_dir_address, pd_index);
        if (!entry_is_present(entry))
            continue;
        
        uintptr_t page_table_address = entry_get_address(entry);
        if (page_table_address == 0)
            continue;

        // free any linked physical pages first
        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            entry = _get_table_entry(page_table_address, pt_index);
            if (!entry_is_present(entry))
                continue;

            phys_addr_t phys_page_address = entry_get_address(entry);
            if (phys_page_address == 0)
                continue;

            // we only free our extra pages, not the kernel ones.
            if (phys_page_address >= kinfo.reserved_area_start && phys_page_address < kinfo.reserved_area_end)
                continue;

            pmm_free_physical_page((phys_addr_t)phys_page_address);
        }

        pmm_free_physical_page((phys_addr_t)page_table_address);
    }

    // we can now free the page directory itself
    pmm_free_physical_page((phys_addr_t)page_dir_address);
    popcli();
}

// --------------------------------------------------------------

void vmm_physpg_read(phys_addr_t paddr, size_t offset, void *buffer, size_t size) {
    // mutex_acquire(&kinfo.work_pages_lock);

    page_dir_t pd = vmm_get_current_page_dir();
    virt_addr_t pa = vmm_resolve(vmm_workpg1(), pd);

    _map_page_primitive(vmm_workpg1(), paddr);
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy(buffer, (void *)vmm_workpg1() + offset, size);

    // mutex_release(&kinfo.work_pages_lock);
}

void vmm_physpg_write(phys_addr_t paddr, size_t offset, void *buffer, size_t size) {
    // mutex_acquire(&kinfo.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy((void *)vmm_workpg1() + offset, buffer, size);

    // mutex_release(&kinfo.work_pages_lock);
}

void vmm_physpg_clear(phys_addr_t paddr) {
    // mutex_acquire(&kinfo.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    memset((void *)vmm_workpg1(), 0, vmm_page_size());

    // mutex_release(&kinfo.work_pages_lock);
}

uint32_t vmm_physpg_get_entry(phys_addr_t paddr, int index) {
    // mutex_acquire(&kinfo.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    index = clamp(index, 0, 1023);
    return ((uint32_t *)vmm_workpg1())[index];

    // mutex_release(&kinfo.work_pages_lock);
}

void vmm_physpg_set_entry(phys_addr_t paddr, int index, uint32_t value) {
    // mutex_acquire(&kinfo.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), paddr);
    index = clamp(index, 0, 1023);
    ((uint32_t *)vmm_workpg1())[index] = value;

    // mutex_release(&kinfo.work_pages_lock);
}

void vmm_physpg_copy(phys_addr_t pdest, virt_addr_t psource) {
    // mutex_acquire(&kinfo.work_pages_lock);

    _map_page_primitive(vmm_workpg1(), pdest);
    _map_page_primitive(vmm_workpg2(), psource);
    memcpy((void *)vmm_workpg1(), (void *)vmm_workpg2(), vmm_page_size());

    // mutex_release(&kinfo.work_pages_lock);
}

// --------------------------------------------------------------

// RMW = Recursive Mapping Window, the top 4MB of virtual memory
// we enable this by mapping EVERY page directory to 0xFFC00000
// it allows us to manipulate mapping transparently, in EVERY page directory.

virt_addr_t rmw_base_address()        { return 0xFFC00000; } // 4GB - 4MB
virt_addr_t rmw_pd_address()          { return 0xFFFFF000; } // 4GB - 4KB (very last page)
virt_addr_t rmw_pt_address(int index) { ASSERT(index >= 0 && index < 1024); return (rmw_base_address() + index * 4096); }

uint32_t rmw_get_pd_entry(int index) {
    ASSERT(index >= 0 && index < 1024);
    return ((uint32_t *)rmw_pd_address())[index];
}

void rmw_set_pd_entry(int index, uint32_t value) {
    ASSERT(index >= 0 && index < 1024);
    ((uint32_t *)rmw_pd_address())[index] = value;
}

uint32_t rmw_get_pt_entry(int pd_index, int pt_index) {
    ASSERT(pd_index >= 0 && pd_index < 1024);
    ASSERT(pt_index >= 0 && pt_index < 1024);
    return ((uint32_t *)rmw_pt_address(pd_index))[pt_index];
}

void rmw_set_pt_entry(int pd_index, int pt_index, uint32_t value) {
    ASSERT(pd_index >= 0 && pd_index < 1024);
    ASSERT(pt_index >= 0 && pt_index < 1024);
    ((uint32_t *)rmw_pt_address(pd_index))[pt_index] = value;
} 

void rmw_setup_page_dir(phys_addr_t page_dir) {
    // every PD points to itself at index 1023, this means that:
    // at virtual address 0xFFFFF000 one can read the PD contents
    // at virtual address 0xFFC00000 + n*4K one can read the PT pages

    if (!kinfo.paging_enabled) {
        // we do it via physical access (pointer)
        ((uint32_t *)page_dir)[1023] = pd_entry_of(page_dir, false, true, true);
    } else {
        // we need the infrastructure of a temp address to map
        // set non-mapped page through the mapping pages
        vmm_workpg1_map_to(page_dir);
        vmm_workpg1_set_entry(1023, page_dir);
        vmm_workpg1_unmap();
    }
}

error_t rmw_map_page(virt_addr_t vaddr, phys_addr_t paddr, bool user, bool writable) {
    // these work for the "current" CR3 only, as long as it's setup recursively

    int pd_idx = page_dir_index(vaddr);
    uint32_t pd_entry = rmw_get_pd_entry(pd_idx);

    if (!entry_is_present(pd_entry)) {
        // allocate new, attach etc
        phys_addr_t new_pt_paddr = pmm_allocate_physical_page();
        if (new_pt_paddr == 0) return ERR_NO_MEMORY;

        pd_entry = pd_entry_of(new_pt_paddr, user, writable, true);
        rmw_set_pd_entry(pd_idx, pd_entry);

        // invalidate the TLB for the RMW window, so the CPU sees the new PT
        vmm_invalidate_cached_address(rmw_pt_address(pd_idx));        
    }

    int pt_idx = page_table_index(vaddr);
    rmw_set_pt_entry(pd_idx, pt_idx, pt_entry_of(paddr, user, writable, true));

    // force CPU to see this address
    vmm_invalidate_cached_address(vaddr);

    return OK;
}

void rmw_unmap_page(virt_addr_t vaddr) {
    // these work for the "current" CR3 only, as long as it's setup recursively

    int pd_idx = page_dir_index(vaddr);
    uint32_t pd_entry = rmw_get_pd_entry(pd_idx);

    if (!entry_is_present(pd_entry)) {
        log_error("unmap page attempt at 0x%x, but no PT found", vaddr);
        return;
    }

    int pt_idx = page_table_index(vaddr);
    rmw_set_pt_entry(pd_idx, pt_idx, 0);

    // force CPU to see this address
    vmm_invalidate_cached_address(vaddr);
}

// ----------------------------------------------

