#pragma once

#include "../include/ctypes.h"
#include "../include/uapi/errors.h"


#define PAGE_SIZE                 4096
#define INVALID_PAGE              0


// this allocator allows for sharing of memory pages
typedef struct page_allocator page_allocator_t;

// this interface to be used by virtual memory manager and the virtual memory areas 
struct page_allocator {
    // allocates contiguous pages, returns start address or zero.
    // initializes usage counter to 1
    phys_addr_t (*allocate_pages)(page_allocator_t *self, size_t num_pages);

    // increases usage counter by 1, used by processes sharing memory page
    error_t (*retain_page)(page_allocator_t *self, phys_addr_t page_address);

    // decreases counter by 1, if zero, page is back to allocatable
    error_t (*release_page)(page_allocator_t *self, phys_addr_t page_address);

    // allocator private data
    void *private_data;
};

typedef uintptr_t addr_t;

typedef struct region {
    addr_t base;
    size_t size;
} region_t;

page_allocator_t *create_page_allocator(
    uint64_t highest_memory_address,  // how much memory do we have? supporting up to 4GB
    region_t allocator_workspace,       // inside kernel, predefined 2MB for counters, later identity mapped
    region_t *available_regions,        // from BIOS discovery
    size_t num_available_regions,
    region_t *reserved_regions,         // kernel, etc
    size_t num_reserved_regions
);
