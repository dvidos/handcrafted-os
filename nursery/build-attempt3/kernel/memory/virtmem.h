#ifndef _VIRTMEM_H
#define _VIRTMEM_H

#include "../include/ctypes.h"
#include "../memory/physmem.h"
#include "../memory/mem_region.h"
#include "../memory/mem_map.h"
#include "../arch/trap_frame.h"

static inline uint32_t vmm_page_size()                         { return PAGE_SIZE; }
static inline uint32_t vmm_round_up(uint32_t address)          { return ((address + vmm_page_size() - 1) / vmm_page_size()) * vmm_page_size(); }
static inline uint32_t vmm_round_down(uint32_t address)        { return ((address                      ) / vmm_page_size()) * vmm_page_size(); }
static inline uint32_t vmm_is_page_aligned(uint32_t address)   { return address == vmm_round_down(address); }
static inline uint32_t vmm_pages_for_size(uint32_t size)       { return vmm_round_up(size) / vmm_page_size(); }
static inline uint32_t vmm_page_address(uint32_t address)      { return address & 0xFFFFF000; }
static inline uint32_t vmm_page_offset(uint32_t address)       { return address & 0x00000FFF; }


// initialize virtual memory paging.
void vmm_initialize(phys_addr_t kernel_reserved_area_start, phys_addr_t kernel_reserved_area_end, phys_addr_t utility_pages_addr, size_t utility_pages_size);

// used for loading processes and fork()
virt_addr_t vmm_get_kernel_area_end();

// resolve a virtual address, by reading the page dir and tables
virt_addr_t vmm_resolve(virt_addr_t virtual_addr, page_dir_t page_dir_addr);


// map / unmap pages to current or given page directory
error_t vmm_map_page_to_pd(virt_addr_t virtual_addr, phys_addr_t physical_addr, bool user_accessible, bool write_enable, page_dir_t page_dir);
void    vmm_unmap_page_from_pd(virt_addr_t virtual_addr, page_dir_t page_dir);

// invalidate TLB cache
void vmm_invalidate_cached_address(virt_addr_t virtual_addr);

// identity map a whole range of addresses
error_t vmm_identity_map_range(phys_addr_t start_addr, phys_addr_t end_addr, page_dir_t page_dir_addr);

// return the page direcrory address for the kernel
page_dir_t vmm_get_kernel_page_directory();

// to be called upon page fault interrupt
void vmm_page_fault_handler(trap_frame_t *tf);

// get/set bit 31 of CR0 register
void vmm_enable_paging();
void vmm_disable_paging();

// read/write the CR3 register
void vmm_set_page_directory_register(page_dir_t address);
page_dir_t vmm_get_current_page_dir();



// allocates and creates a new page directory
page_dir_t vmm_create_page_directory(bool map_kernel_space);

// allocates pages and maps them to the virtual addresses requested (end_addr exclusive)
error_t vmm_allocate_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr);

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_page_directory(page_dir_t page_dir_address);

// logs the virtual to physical mapping that a page directory causes
void vmm_dump_page_directory(virt_addr_t page_dir_address);
void vmm_pagedir_log_formatter(log_write_stream_t *stream, va_list args);


// -------------------------------------

typedef struct vmm_page_ops {
    void     (*read)(virt_addr_t paddr, size_t offset, void *buffer, size_t size);
    void     (*write)(virt_addr_t paddr, size_t offset, void *buffer, size_t size);
    void     (*clear)(virt_addr_t paddr);
    uint32_t (*get_entry)(virt_addr_t paddr, int index);
    void     (*set_entry)(virt_addr_t paddr, int index, uint32_t value);
    void     (*copy)(virt_addr_t pdest, virt_addr_t psource);
} vmm_page_ops_t;

vmm_page_ops_t *vmm_page_ops_for(page_dir_t page_dir);

void     vmm_physpg_read(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
void     vmm_physpg_write(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
void     vmm_physpg_clear(phys_addr_t paddr);
uint32_t vmm_physpg_get_entry(phys_addr_t paddr, int index);
void     vmm_physpg_set_entry(phys_addr_t paddr, int index, uint32_t value);
void     vmm_physpg_copy(phys_addr_t pdest, virt_addr_t psource);

void     vmm_direct_physpg_read(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
void     vmm_direct_physpg_write(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
void     vmm_direct_physpg_clear(phys_addr_t paddr);
uint32_t vmm_direct_physpg_get_entry(phys_addr_t paddr, int index);
void     vmm_direct_physpg_set_entry(phys_addr_t paddr, int index, uint32_t value);
void     vmm_direct_physpg_copy(phys_addr_t pdest, virt_addr_t psource);



#endif
