#ifndef _VIRTMEM_H
#define _VIRTMEM_H

#include "../include/ctypes.h"
#include "physmem.h"


typedef uintptr_t page_dir_t;   // address of a page_directory, for CR3 register
typedef uintptr_t virt_addr_t;  // virtual address. 32 bits -> 4 GB



// resolve a virtual address, by reading the page dir and tables
phys_addr_t resolve_virtual_to_physical_address(virt_addr_t virtual_addr, page_dir_t page_dir_addr);

// map a virtual address to a physical one
void map_virtual_address_to_physical(virt_addr_t virtual_addr, phys_addr_t physical_addr, page_dir_t page_dir, bool user_accessible, bool write_enable);

// unmap a virtual address (remove paging entries)
void unmap_virtual_address(virt_addr_t virtual_addr, page_dir_t page_dir_addr);

// invalidate TLB cache
void invalidate_paging_cached_address(virt_addr_t virtual_addr);

// identity map a whole range of addresses
void identity_map_range(phys_addr_t start_addr, phys_addr_t end_addr, page_dir_t page_dir_addr);

// initialize virtual memory paging.
void init_virtual_memory_paging(phys_addr_t kernel_start_address, phys_addr_t kernel_end_address);

// return the page direcrory address for the kernel
page_dir_t get_kernel_page_directory();

// to be called upon page fault interrupt
void virtual_memory_page_fault_handler(uint32_t error_code);


// write the CR3 register
void set_page_directory_register(page_dir_t address);

// read the CR3 register
page_dir_t get_page_directory_register();



// allocates and creates a new page directory
page_dir_t create_page_directory(bool map_kernel_space);

// allocates pages and maps them to the virtual addresses requested (end_addr exclusive)
void allocate_virtual_memory_range(virt_addr_t virt_addr_start, virt_addr_t virt_addr_end, page_dir_t page_dir_addr);

// frees any pointed pages, page tables, and the page directory itself
void destroy_page_directory(page_dir_t page_dir_address);

// logs the virtual to physical mapping that a page directory causes
void dump_page_directory(virt_addr_t page_dir_address);



#endif
