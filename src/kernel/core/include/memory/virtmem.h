#ifndef _VIRTMEM_H
#define _VIRTMEM_H


// resolve a virtual address, by reading the page dir and tables
void *resolve_virtual_to_physical_address(void *virtual_addr, void *page_dir_addr);

// map a virtual address to a physical one
void map_virtual_address_to_physical(void *virtual_addr, void *physical_addr, void *page_dir_addr);

// unmap a virtual address (remove paging entries)
void unmap_virtual_address(void *virtual_addr, void *page_dir_addr);

// identity map a whole range of addresses
void identity_map_range(void *start_addr, void *end_addr, void *page_dir_addr);

// initialize virtual memory paging.
void init_virtual_memory_paging(void *kernel_start_address, void *kernel_end_address);

// to be called upon page fault interrupt
void virtual_memory_page_fault_handler(uint32_t error_code);

#endif
