#ifndef _VIRTMEM_H
#define _VIRTMEM_H

#include <ctypes.h>

// resolve a virtual address, by reading the page dir and tables
void *resolve_virtual_to_physical_address(void *virtual_addr, void *page_dir_addr);

// map a virtual address to a physical one
void map_virtual_address_to_physical(void *virtual_addr, void *physical_addr, void *page_dir_addr, bool skip_logging);

// unmap a virtual address (remove paging entries)
void unmap_virtual_address(void *virtual_addr, void *page_dir_addr);

// invalidate TLB cache
void invalidate_paging_cached_address(void *virtual_addr);

// identity map a whole range of addresses
void identity_map_range(void *start_addr, void *end_addr, void *page_dir_addr);

// initialize virtual memory paging.
void init_virtual_memory_paging(void *kernel_start_address, void *kernel_end_address);

// return the page direcrory address for the kernel
void *get_kernel_page_directory();

// to be called upon page fault interrupt
void virtual_memory_page_fault_handler(uint32_t error_code);


// write the CR3 register
void set_page_directory_register(void *address);

// read the CR3 register
void *get_page_directory_register();



// allocates and creates a new page directory
void *create_page_directory(bool map_kernel_space);

// allocates pages and maps them to the virtual addresses requested (end_addr exclusive)
void allocate_virtual_memory_range(void *virt_addr_start, void *virt_addr_end, void *page_dir_addr);

// frees any pointed pages, page tables, and the page directory itself
void destroy_page_directory(void *page_dir_address);

// logs the virtual to physical mapping that a page directory causes
void dump_page_directory(void *page_dir_address);



#endif
