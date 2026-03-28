#include "vmm_internal.h"





// ---------------------------------------------------------------------


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

// ---------------------------------------------------------------------

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

