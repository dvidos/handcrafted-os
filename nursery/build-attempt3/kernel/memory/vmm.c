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

static void vmm_create_kernel_page_directory_using_mapping_pages(page_dir_t kernel_pd, virt_addr_t start_addr, virt_addr_t end_addr);


// -------------------------------------------------------------


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
    kinfo.diags_page_addr = vmm_allocate_kernel_mapping_page();
    log_debug("kernel page_dir=0x%x, work_page1=0x%x, work_page2=0x%x, diags_page=%p", kinfo.page_directory, kinfo.work_page1_addr, kinfo.work_page2_addr, kinfo.diags_page_addr);

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

phys_addr_t vmm_resolve(virt_addr_t vaddr, page_dir_t page_dir_addr) {
    // For each virtual address, when we are dealing with 4K pages:
    //     10 bits 31-22 dictate the page directory entry (we find the table)
    //     10 bits 21-12 dictate the page table entry (we find the page)
    //     12 bits 11-0  dictate the byte within the page (12 bits address a 4KB space)


    // 1. Get the PD entry for the target virtual address
    uint32_t pde = vmm_physpg_get_entry(page_dir_addr, page_dir_index(vaddr));
    if (!entry_is_present(pde)) return 0;

    // 2. The PDE points to the physical address of a Page Table
    phys_addr_t pt_paddr = entry_get_address(pde);
    
    // 3. Get the PT entry
    uint32_t pte = vmm_physpg_get_entry(pt_paddr, page_table_index(vaddr));
    if (!entry_is_present(pte)) return 0;

    return entry_get_address(pte) + vmm_page_offset(vaddr);
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
    uint32_t entry = vmm_physpg_get_entry(page_dir, index);
    if (!entry_is_present(entry))
        return;

    // map/update/unmap to clear the entry in the PT
    phys_addr_t page_table_paddr = entry_get_address(entry);
    index = page_table_index(virtual_addr);
    vmm_physpg_set_entry(page_table_paddr, index, 0);
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



page_dir_t vmm_get_kernel_page_directory() {
    return kinfo.page_directory;
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

