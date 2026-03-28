#include "vmm_internal.h"


struct _pd_group {
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t size;
    char permissions[4];
};

static int _pd_formatter_mapped_pt_index;
static virt_addr_t _pd_formatter_mapped_pt_address;

// ------------------------------------------------------------

static void *vmm_diags_page_map(phys_addr_t p_target) {
    // no locks here, to allow logger to see things
    // uses a dedicated mapping page of the kernel, to avoid messing with current state

    int pd_idx = page_dir_index(kinfo.diags_page_addr);
    int pt_idx = page_table_index(kinfo.diags_page_addr);
    
    uint32_t entry = pt_entry_of(p_target, false, false, true);
    rmw_set_pt_entry(pd_idx, pt_idx, entry);
    vmm_invalidate_cached_address(kinfo.diags_page_addr);

    return (void *)kinfo.diags_page_addr;
}

static void vmm_diags_page_unmap() {
    int pd_idx = page_dir_index(kinfo.diags_page_addr);
    int pt_idx = page_table_index(kinfo.diags_page_addr);
    
    rmw_set_pt_entry(pd_idx, pt_idx, 0);
    vmm_invalidate_cached_address(kinfo.diags_page_addr);
}

static uint32_t vmm_diags_page_get_pd_entry(uint32_t requested_pd, int index) {
    // Map the PD to the private diagnostic window
    uint32_t *pd_ptr = (uint32_t *)vmm_diags_page_map(requested_pd);
    uint32_t entry = pd_ptr[index];
    
    // Clean up immediately so the window is ready for the Page Table next
    vmm_diags_page_unmap(); 
    
    return entry;
}

// ------------------------------------------------------------


static void formatter_group_init(struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {
    g->vaddr = vaddr;
    g->paddr = paddr;
    g->size = 0;
    g->permissions[0] = pd_usr ? 'U' : 'S';
    g->permissions[1] = pd_wrt ? 'W' : 'R';
    g->permissions[2] = pt_usr ? 'U' : 'S';
    g->permissions[3] = pt_wrt ? 'W' : 'R';
}

static bool formatter_group_is_extension(struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {
    if (vaddr != g->vaddr + g->size) return false;
    if (paddr != g->paddr + g->size) return false;
    char tmp[4];
    tmp[0] = pd_usr ? 'U' : 'S';
    tmp[1] = pd_wrt ? 'W' : 'R';
    tmp[2] = pt_usr ? 'U' : 'S';
    tmp[3] = pt_wrt ? 'W' : 'R';
    if (memcmp(g->permissions, tmp, 4) != 0) return false;
    return true;
}

static void formatter_group_extend(struct _pd_group *g) {
    g->size += vmm_page_size();
}

static void formatter_group_print(log_write_stream_t *stream, struct _pd_group *g, bool print_empty_groups) {
    // Virtual address          Physical address         Size KB  PD  PT  Identity
    // 0x12345xxx..0x12345xxx   0x12345xxx..0x12345xxx   1234567  XX  XX  identity
    if (g == NULL) {
        stream->printf(stream, "Virtual address          Physical address         Size KB  PD  PT  Identity");
    } else {
        if (g->size == 0 && !print_empty_groups) 
            return; // ignore empty groups

        stream->printf(stream, "0x%08x..0x%08x   0x%08x..0x%08x   %7d  %c%c  %c%c  %s",
            g->vaddr, g->vaddr + g->size - 1,
            g->paddr, g->paddr + g->size - 1,
            (int)(g->size / 1024),
            g->permissions[0],
            g->permissions[1],
            g->permissions[2],
            g->permissions[3],
            g->vaddr == g->paddr ? "identity" : "-"
        );
    }
}

static void formatter_got_mapping(log_write_stream_t *stream, struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {

    if (!formatter_group_is_extension(g, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr)) {
        formatter_group_print(stream, g, false);
        formatter_group_init(g, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr);
    }
    formatter_group_extend(g);
}

static uint32_t formatter_retrieve_pd_entry(bool is_current_pd, uint32_t requested_pd, int index) {
    if (!kinfo.paging_enabled) {
        // since paging not enable, direct access to pointer
        return ((uint32_t *)requested_pd)[index];
    } else if (is_current_pd) {
        // the current PD can always be seen in the Recursive Mapping Window
        return rmw_get_pd_entry(index);
    } else {
        // a foreign PD will be mapped on work page 1, to be seen there
        return vmm_diags_page_get_pd_entry(requested_pd, index);
    }
}

static uint32_t formatter_get_pt_address(bool is_current_pd, uint32_t pd_value, int pd_index) {
    ASSERT(pd_index >= 0 && pd_index < 1024);

    if (!kinfo.paging_enabled) {
        // we support direct pointer access
        return entry_get_address(pd_value);
    } else if (is_current_pd) {
        // the current PTs can always be seen in the Recursive Mapping Window
        return rmw_pt_address(pd_index);
    } else {
        // we'll use the dedicated diags page to see
        return (uint32_t)vmm_diags_page_map(entry_get_address(pd_value));
    }
}

void vmm_pagedir_log_formatter(log_write_stream_t *stream, va_list args) { 
    pushcli(); // just to avoid race conditions on the pages

    page_dir_t requested_pd = va_arg(args, page_dir_t);
    struct _pd_group grp;
    ASSERT(requested_pd != 0);

    bool is_current_pd = kinfo.paging_enabled && (requested_pd == vmm_get_current_page_dir());

    formatter_group_print(stream, NULL, false); // header
    formatter_group_init(&grp, 0, 0, false, false, false, false);
    
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        uint32_t pd_entry = formatter_retrieve_pd_entry(is_current_pd, requested_pd, pd_index);
        if (!entry_is_present(pd_entry))
            continue;
        
        bool pd_wrt = entry_is_writable(pd_entry);
        bool pd_usr = entry_is_user_accessible(pd_entry);
        uint32_t page_table_vaddr = formatter_get_pt_address(is_current_pd, pd_entry, pd_index);

        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            uint32_t pt_entry = ((uint32_t *)page_table_vaddr)[pt_index];
            if (!entry_is_present(pt_entry))
                continue;
            
            bool pt_wrt = entry_is_writable(pt_entry);
            bool pt_usr = entry_is_user_accessible(pt_entry);
            uint32_t paddr = (uint32_t)entry_get_address(pt_entry);
            uint32_t vaddr = SET_BIT_RANGE(pd_index, 31, 22) | SET_BIT_RANGE(pt_index, 21, 12);

            formatter_got_mapping(stream, &grp, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr);
        }
    }

    formatter_group_print(stream, &grp, true);

    popcli(); // just to avoid race conditions on the pages
}

