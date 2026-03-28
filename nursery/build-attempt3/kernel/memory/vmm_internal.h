#pragma once

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

MODULE("VMM", LOG_LEVEL_INFO);




// this to be included in every page directory we create,
// so that kernel structures and code are always available.
struct kinfo {
    bool paging_enabled;
    page_dir_t page_directory; 

    // area reserved for kernel, identity mapped, usually 0..96MB
    phys_addr_t reserved_area_start;
    phys_addr_t reserved_area_end;
    
    mem_map_t extra_identity_mappings; // we'll see (for example PCI pages added on demand)

    // area for mapping page table entryies and such
    phys_addr_t mapping_pages_addr;
    int mapping_pages_count;
    int mapping_pages_allocated;

    // utility reserved addresses
    virt_addr_t work_page1_addr;
    virt_addr_t work_page2_addr;
    virt_addr_t diags_page_addr; // to be used only by logger.


    lock_t work_pages_lock;

};

extern struct kinfo kinfo;


// ------------------------------------------------------------


static inline uint32_t pd_entry_of(uintptr_t address, bool user_access, bool write_enabled, bool page_present) {

    return
        (((uint32_t)address) & 0xFFFFF000) |
        (((uint32_t)0             & 1) << 4) |   // cache_disable
        (((uint32_t)0             & 1) << 3) |   // write_through
        (((uint32_t)user_access   & 1) << 2) |
        (((uint32_t)write_enabled & 1) << 1) |
        (((uint32_t)page_present  & 1));
}

static inline uint32_t pt_entry_of(uintptr_t address, bool user_access, bool write_enabled, bool page_present) {
    // this value for 4 KB page directory entries
    // accessed set to zero,
    // page size set to zero for 4 KB
    // other bits left to zero
    return
        (((uint32_t)address)        & 0xFFFFF000) | // no shifting here
        (((uint32_t)0               & 0x01) << 8) | // global
        (((uint32_t)0               & 0x01) << 7) | // page_attr_table
        (((uint32_t)0               & 0x01) << 4) | // cache_disable
        (((uint32_t)0               & 0x01) << 3) | // write_through
        (((uint32_t)user_access     & 0x01) << 2) |
        (((uint32_t)write_enabled   & 0x01) << 1) |
        (((uint32_t)page_present    & 0x01));
}

// common to both page directory and page tables
static inline bool entry_is_present(uint32_t entry_value)         { return (entry_value & (1 << 0)); }
static inline bool entry_is_writable(uint32_t entry_value)        { return (entry_value & (1 << 1)); }
static inline bool entry_is_user_accessible(uint32_t entry_value) { return (entry_value & (1 << 2)); }
static inline phys_addr_t entry_get_address(uint32_t entry_value) { return (entry_value & 0xFFFFF000); }



// common to both page directory and page tables
static inline uint32_t _get_table_entry(virt_addr_t table_address, uint32_t index) {
    return ((uint32_t *)table_address)[index];
}

// common to both page directory and page tables
static inline void _set_table_entry(virt_addr_t table_address, uint32_t index, uint32_t value) {
    ((uint32_t *)table_address)[index] = value;
}

// extracts the page directory entry num from a virtual address
static inline int page_dir_index(virt_addr_t address) {
    // highest 10 bits (31-22) are the entry of the page table
    return (int)((((uint32_t)address) >> 22) & 0x3FF);
}

// extracts the page table entry num from a virtual address
static inline int page_table_index(virt_addr_t address) { 
    // second 10 bits (21-12) are the entry of the page table
    return (int)((((uint32_t)address) >> 12) & 0x3FF);
}

// extracts the physical page offset from a virtual address
static inline uint32_t _virt_addr_to_physical_page_offset(virt_addr_t virtual_address) {
    // the lowest 12 (11-0) bits are offset into a 4KB space
    return ((uint32_t)virtual_address) & 0xFFF;
}



// ------ vmm_lower.c --------

// these two are the cornerstone for building higher levels
void _map_page_primitive(virt_addr_t virtual_addr, phys_addr_t physical_addr);
void _unmap_page_primitive(virt_addr_t virtual_addr);

// RMW = Recursive Mapping Window, the top 4MB of virtual memory. 
// we enable this by mapping EVERY page directory we create to 0xFFC00000
// it allows us to manipulate mapping transparently, in EVERY page directory.
static inline virt_addr_t rmw_base_address()                                           { return 0xFFC00000; } // 4GB - 4MB
static inline virt_addr_t rmw_pd_address()                                             { return 0xFFFFF000; } // 4GB - 4KB (very last page)
static inline virt_addr_t rmw_pt_address(int index)                                    { ASSERT(index >= 0 && index < 1024); return (rmw_base_address() + index * 4096); }
static inline uint32_t    rmw_get_pd_entry(int index)                                  { ASSERT(index >= 0 && index < 1024); return ((uint32_t *)rmw_pd_address())[index]; }
static inline void        rmw_set_pd_entry(int index, uint32_t value)                  { ASSERT(index >= 0 && index < 1024); ((uint32_t *)rmw_pd_address())[index] = value; }
static inline uint32_t    rmw_get_pt_entry(int pd_index, int pt_index)                 { ASSERT(pd_index >= 0 && pd_index < 1024); ASSERT(pt_index >= 0 && pt_index < 1024); return ((uint32_t *)rmw_pt_address(pd_index))[pt_index]; }
static inline void        rmw_set_pt_entry(int pd_index, int pt_index, uint32_t value) { ASSERT(pd_index >= 0 && pd_index < 1024); ASSERT(pt_index >= 0 && pt_index < 1024); ((uint32_t *)rmw_pt_address(pd_index))[pt_index] = value; } 
error_t rmw_map_page(virt_addr_t vaddr, phys_addr_t paddr, bool user, bool writable);
void    rmw_unmap_page(virt_addr_t vaddr);


