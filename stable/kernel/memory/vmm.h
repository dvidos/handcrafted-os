#ifndef _VIRTMEM_H
#define _VIRTMEM_H

#include "../include/ctypes.h"
#include "../memory/physmem.h"
#include "../memory/mem_region.h"
#include "../memory/mem_map.h"
#include "../arch/stack_frames.h"

static inline virt_addr_t vmm_rmw_base_address()               { return 0xFFC00000; } // 4GB - 4MB
static inline virt_addr_t vmm_rmw_pd_address()                 { return 0xFFFFF000; } // 4GB - 4KB (very last page)

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


// mapping window for current PD, temp mapping pages for foreign PDs
error_t vmm_map_page_to_current_pd(virt_addr_t virtual_addr, virt_addr_t physical_addr, bool user_accessible, bool write_enable);
error_t vmm_map_page_to_other_pd(virt_addr_t virtual_addr, virt_addr_t physical_addr, bool user_accessible, bool write_enable, page_dir_t page_dir);
void    vmm_unmap_page_from_current_pd(virt_addr_t virtual_addr);
void    vmm_unmap_page_from_other_pd(virt_addr_t virtual_addr, page_dir_t page_dir);


// invalidate TLB cache
void vmm_invalidate_cached_address(virt_addr_t virtual_addr);

// identity map a whole range of addresses
error_t vmm_map_mem_io(phys_addr_t start_addr, size_t length, page_dir_t page_dir_addr);

// return the page direcrory address for the kernel
page_dir_t vmm_get_kernel_page_directory();

// to be called upon page fault interrupt
void vmm_page_fault_handler(interrupt_frame_t *frame);

// get/set bit 31 of CR0 register
void vmm_enable_paging();
void vmm_disable_paging();

// read/write the CR3 register
void vmm_set_page_directory_register(page_dir_t address);
page_dir_t vmm_get_current_page_dir();



// allocates and creates a new page directory
page_dir_t vmm_create_user_page_directory();

// allocates pages and maps them to the virtual addresses requested (end_addr exclusive)
error_t vmm_allocate_memory_range_this_pd(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end);

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_user_page_directory(page_dir_t page_dir_address);

// logs the virtual to physical mapping that a page directory causes
void vmm_pagedir_log_formatter(log_write_stream_t *stream, va_list args);



// vmm_low.c: these use internal work pages and are thread safe
void     vmm_physpg_clear(phys_addr_t paddr);
void     vmm_physpg_read(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
void     vmm_physpg_write(phys_addr_t paddr, size_t offset, void *buffer, size_t size);
uint32_t vmm_physpg_get_entry(phys_addr_t paddr, int index);
void     vmm_physpg_set_entry(phys_addr_t paddr, int index, uint32_t value);
void     vmm_physpg_copy(phys_addr_t pdest, virt_addr_t psource);



#endif
