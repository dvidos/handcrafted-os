#include "vmm_internal.h"





// ---------------------------------------------------------------------

// INTERNAL: The "Naked" Mapper (no Locks, no.. well some checks)

static bool pg1_used = false;
static bool pg2_used = false;


static void *_low_mount_internal(virt_addr_t v_window, phys_addr_t p_target) {
    ASSERT(kinfo.paging_enabled);
    ASSERT(vmm_is_page_aligned(v_window));
    ASSERT(vmm_is_page_aligned(p_target));
    ASSERT(v_window == kinfo.work_page1_addr || v_window == kinfo.work_page2_addr);

    // if (v_window == kinfo.work_page1_addr && pg1_used) panic("Re-entrance in _low_mount() for page 1");
    // if (v_window == kinfo.work_page2_addr && pg2_used) panic("Re-entrance in _low_mount() for page 2");

    int pd_idx = page_dir_index(v_window);
    int pt_idx = page_table_index(v_window);
    
    // using RMW logic to point the window at the physical page
    uint32_t entry = pt_entry_of(p_target, false, true, true);
    rmw_set_pt_entry(pd_idx, pt_idx, entry);
    vmm_invalidate_cached_address(v_window);

    if (v_window == kinfo.work_page1_addr) pg1_used = true;
    if (v_window == kinfo.work_page2_addr) pg2_used = true;

    return (void *)v_window;
}

static void *_low_unmount_internal(virt_addr_t v_window) {
    ASSERT(kinfo.paging_enabled);
    ASSERT(vmm_is_page_aligned(v_window));
    ASSERT(v_window == kinfo.work_page1_addr || v_window == kinfo.work_page2_addr);

    int pd_idx = page_dir_index(v_window);
    int pt_idx = page_table_index(v_window);
    
    rmw_set_pt_entry(pd_idx, pt_idx, 0);
    vmm_invalidate_cached_address(v_window);

    if (v_window == kinfo.work_page1_addr) pg1_used = false;
    if (v_window == kinfo.work_page2_addr) pg2_used = false;

    return (void *)v_window;
}

// ---------------------------------------------------------------------

// PUBLIC: The Thread-Safe Accessors (this hides pg1/pg2 from rest of kernel)

void vmm_physpg_clear(phys_addr_t paddr) {
    pushcli();

    memset(_low_mount_internal(kinfo.work_page1_addr, paddr), 0, vmm_page_size());
    _low_unmount_internal(kinfo.work_page1_addr);
    
    popcli();
}

void vmm_physpg_read(phys_addr_t paddr, size_t offset, void *buffer, size_t size) {
    pushcli();

    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy(buffer, _low_mount_internal(kinfo.work_page1_addr, paddr) + offset, size);
    _low_unmount_internal(kinfo.work_page1_addr);

    popcli();
}

void vmm_physpg_write(phys_addr_t paddr, size_t offset, void *buffer, size_t size) {
    pushcli();

    offset = min(offset, vmm_page_size());
    size = min(size, vmm_page_size() - offset);
    memcpy(_low_mount_internal(kinfo.work_page1_addr, paddr) + offset, buffer, size);
    _low_unmount_internal(kinfo.work_page1_addr);

    popcli();
}

uint32_t vmm_physpg_get_entry(phys_addr_t paddr, int index) {
    pushcli();

    index = clamp(index, 0, 1023);
    uint32_t entry = ((uint32_t *)_low_mount_internal(kinfo.work_page1_addr, paddr))[index];
    _low_unmount_internal(kinfo.work_page1_addr);

    popcli();
    return entry;
}

void vmm_physpg_set_entry(phys_addr_t paddr, int index, uint32_t value) {
    pushcli();

    index = clamp(index, 0, 1023);
    ((uint32_t *)_low_mount_internal(kinfo.work_page1_addr, paddr))[index] = value;
    _low_unmount_internal(kinfo.work_page1_addr);

    popcli();
}

void vmm_physpg_copy(phys_addr_t pdest, virt_addr_t psource) {
    pushcli();

    memcpy(_low_mount_internal(kinfo.work_page1_addr, pdest), _low_mount_internal(kinfo.work_page2_addr, psource), vmm_page_size());
    _low_unmount_internal(kinfo.work_page1_addr);

    popcli();
}

// --------------------------------------------------------------

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

// ---------------------------------

