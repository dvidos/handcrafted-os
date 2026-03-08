#ifndef _VIRTMEM_H
#define _VIRTMEM_H

#include "../include/ctypes.h"
#include "physmem.h"


typedef uintptr_t page_dir_t;   // address of a page_directory, for CR3 register
typedef uintptr_t virt_addr_t;  // virtual address. 32 bits -> 4 GB



// resolve a virtual address, by reading the page dir and tables
phys_addr_t vmm_resolve(virt_addr_t virtual_addr, page_dir_t page_dir_addr);

// map a virtual address to a physical one
void vmm_map_virtual_to_physical(virt_addr_t virtual_addr, phys_addr_t physical_addr, page_dir_t page_dir, bool user_accessible, bool write_enable);

// unmap a virtual address (remove paging entries)
void vmm_unmap(virt_addr_t virtual_addr, page_dir_t page_dir_addr);

// invalidate TLB cache
void vmm_invalidate_cached_address(virt_addr_t virtual_addr);

// identity map a whole range of addresses
void vmm_identity_map_range(phys_addr_t start_addr, phys_addr_t end_addr, page_dir_t page_dir_addr);

// initialize virtual memory paging.
void vmm_initialize(phys_addr_t kernel_start_address, phys_addr_t kernel_end_address);

// return the page direcrory address for the kernel
page_dir_t vmm_get_kernel_page_directory();

// to be called upon page fault interrupt
void vmm_page_fault_handler(uint32_t error_code);


// write the CR3 register
void vmm_set_page_directory_register(page_dir_t address);

// read the CR3 register
page_dir_t vmm_get_page_directory_register();



// allocates and creates a new page directory
page_dir_t vmm_create_page_directory(bool map_kernel_space);

// allocates pages and maps them to the virtual addresses requested (end_addr exclusive)
void vmm_allocate_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr);

// frees any pointed pages, page tables, and the page directory itself
void vmm_destroy_page_directory(page_dir_t page_dir_address);

// logs the virtual to physical mapping that a page directory causes
void vmm_dump_page_directory(virt_addr_t page_dir_address);



#endif
