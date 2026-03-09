#pragma once

#include "../include/ctypes.h"

// maybe i need a better base abstraction for panic. 
// dedicated header and source file, not needing the VGA driver.
// we could have the "panic" function just halt, 
// and then add a "panic_set_message_displayer()" method.
// this way, we depend on a linked-in panic() method, but nothing else.



// to be used as a dependency to other systems
typedef struct pmm_allocator_t {
    phys_addr_t (*allocate_physical_page)();
    void (*free_physical_page)(phys_addr_t addr);
} pmm_allocator_t;


void pmm_initialize(uint64_t highest_address, phys_addr_t kernel_end);
void pmm_mark_region_available(phys_addr_t start, size_t length);
void pmm_mark_region_reserved(phys_addr_t start, size_t length);
void pmm_finish_initialization();

phys_addr_t pmm_allocate_physical_page();
void pmm_free_physical_page(phys_addr_t addr);
phys_addr_t pmm_allocate_consecutive_pages(size_t total_bytes);
void pmm_free_consecutive_pages(phys_addr_t address, size_t total_bytes);

uint32_t pmm_total_pages();
uint32_t pmm_free_pages();
uint32_t pmm_used_pages();

pmm_allocator_t pmm_get_pmm_allocator();
phys_addr_t pmm_get_top_identity_address();
void pmm_debug_bitmap_ranges();



#define PAGE_SIZE                 4096
#define INVALID_PAGE              0
#define BYTES_TO_PAGES(bytes)     (((bytes) + PAGE_SIZE - 1) / PAGE_SIZE)

extern struct pmm_ops pmm;
