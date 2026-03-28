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


struct _pd_group {
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t size;
    char permissions[4];
};

static int _pd_formatter_mapped_pt_index;


static void _pd_formatter_group_init(struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {
    g->vaddr = vaddr;
    g->paddr = paddr;
    g->size = 0;
    g->permissions[0] = pd_usr ? 'U' : 'S';
    g->permissions[1] = pd_wrt ? 'W' : 'R';
    g->permissions[2] = pt_usr ? 'U' : 'S';
    g->permissions[3] = pt_wrt ? 'W' : 'R';
}

static bool _pd_formatter_group_is_extension(struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {
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

static void _pd_formatter_group_extend(struct _pd_group *g) {
    g->size += vmm_page_size();
}

static void _pd_formatter_group_print(log_write_stream_t *stream, struct _pd_group *g, bool print_empty_groups) {
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

static void _pd_formatter_got_mapping(log_write_stream_t *stream, struct _pd_group *g, uint32_t vaddr, uint32_t paddr, bool pd_wrt, bool pd_usr, bool pt_wrt, bool pt_usr) {
    if (!_pd_formatter_group_is_extension(g, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr)) {
        _pd_formatter_group_print(stream, g, false);
        _pd_formatter_group_init(g, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr);
    }
    _pd_formatter_group_extend(g);
}

static uint32_t _pd_formatter_get_accessible_pd_address(bool is_current_pd, uint32_t requested_pd) {
    if (!kinfo.paging_enabled) {
        // since paging not enable, direct access to pointer
        return requested_pd;
    } else if (is_current_pd) {
        // the current PD can always be seen in the Recursive Mapping Window
        return rmw_pd_address();
    } else {
        // a foreign PD will be mapped on work page 1, to be seen there
        vmm_workpg1_map_to(requested_pd);
        _pd_formatter_mapped_pt_index = -1;
        return vmm_workpg1();
    }
}

static uint32_t _pd_formatter_get_accessible_pt_address(bool is_current_pd, uint32_t accessible_pd, int pt_index) {
    ASSERT(accessible_pd != 0);
    ASSERT(pt_index >= 0 && pt_index < 1024);


    if (!kinfo.paging_enabled) {
        // we support direct pointer access
        uint32_t pd_value = ((uint32_t *)accessible_pd)[pt_index];
        return entry_get_address(pd_value);
    } else if (is_current_pd) {
        // the current PTs can always be seen in the Recursive Mapping Window
        return rmw_pt_address(pt_index);
    } else {
        // we need to map this to work page2
        if (pt_index != _pd_formatter_mapped_pt_index) {
            uint32_t pd_value = ((uint32_t *)accessible_pd)[pt_index];
            uint32_t pt_addr = entry_get_address(pd_value);
            vmm_workpg2_map_to(pt_addr);
            _pd_formatter_mapped_pt_index = pt_index;
        }
        return vmm_workpg2();
    }
}

void vmm_pagedir_log_formatter(log_write_stream_t *stream, va_list args) { 
    page_dir_t requested_pd = va_arg(args, page_dir_t);
    struct _pd_group grp;
    ASSERT(requested_pd != 0);

    log_info("pd_formatter, curr_pd=0x%x, target=0x%x", vmm_get_current_page_dir(), requested_pd);

    // if this is the same PD, we can use wmr functions.
    // if it's is different, we should use physpg functions
    // but physpg functions work only on kernel PD ???
    // and then, first time we print, paging is not enabled !!!!!!!!!!

    // another way is to set a page address every time.
    // - before paging: the physical address
    // - mapping: the mapping address
    // - rmw: the address of the 0xFFC000000+(n*4KB)

    bool is_current_pd = kinfo.paging_enabled && (requested_pd == vmm_get_current_page_dir());
    uint32_t accessible_pd = _pd_formatter_get_accessible_pd_address(is_current_pd, requested_pd);


    _pd_formatter_group_print(stream, NULL, false); // header
    _pd_formatter_group_init(&grp, 0, 0, false, false, false, false);
    for (int pd_index = 0; pd_index < 1024; pd_index++) {
        uint32_t pd_entry = ((uint32_t *)accessible_pd)[pd_index];
        if (!entry_is_present(pd_entry))
            continue;
        
        bool pd_wrt = entry_is_writable(pd_entry);
        bool pd_usr = entry_is_user_accessible(pd_entry);
        uint32_t accessible_pt = _pd_formatter_get_accessible_pt_address(is_current_pd, accessible_pd, pd_index);

        for (int pt_index = 0; pt_index < 1024; pt_index++) {
            uint32_t pt_entry = ((uint32_t *)accessible_pt)[pt_index];
            if (!entry_is_present(pt_entry))
                continue;
            
            bool pt_wrt = entry_is_writable(pt_entry);
            bool pt_usr = entry_is_user_accessible(pt_entry);
            uint32_t paddr = (uint32_t)entry_get_address(pt_entry);
            uint32_t vaddr = SET_BIT_RANGE(pd_index, 31, 22) | SET_BIT_RANGE(pt_index, 21, 12);

            _pd_formatter_got_mapping(stream, &grp, vaddr, paddr, pd_wrt, pd_usr, pt_wrt, pt_usr);
        }
    }
    _pd_formatter_group_print(stream, &grp, true);
}

