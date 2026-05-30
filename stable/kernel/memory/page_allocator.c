#include "page_allocator.h"
#include "../logger/logger.h"
#include "../utils/panic.h"

MODULE("PG_ALLOC", LOG_LEVEL_INFO);

typedef struct allocator_priv_data allocator_priv_data_t;

struct allocator_priv_data {
    uint64_t highest_memory_address;

    // pointer to array of 1M of 16 bit pointers (total 2MB of memory)
    uint16_t *page_refs;
    uint32_t num_pages;

    // which page to check next
    uint32_t next_allocation_hint; 
};

#define FREE                         0
#define RESERVED                     0xFFFF

#define ADDR_TO_PAGE_NO(addr)        ((addr) >> 12)
#define PAGE_NO_TO_ADDR(page_no)     ((page_no) << 12)
#define SIZE_TO_NUM_PAGES(bytes)     (((bytes) + 0xFFF) >> 12)

// ------------------------------------------------------------------

static bool consecutive_pages_free(allocator_priv_data_t *data, size_t first_page, size_t num_pages) {
    for (size_t i = 0; i < num_pages; i++)
        if (data->page_refs[first_page + i] > 0)
            return false;
    
    return true;
}

static void set_consecutive_pages_value(allocator_priv_data_t *data, size_t first_page, size_t num_pages, uint16_t value) {
    for (size_t i = 0; i < num_pages; i++)
        data->page_refs[first_page + i] = value;
}

static void set_region_ref_value(allocator_priv_data_t *data, region_t region, uint16_t value) {
    set_consecutive_pages_value(data, ADDR_TO_PAGE_NO(region.base), SIZE_TO_NUM_PAGES(region.size), value);
}

// -------------------------------------------------------------------

static phys_addr_t page_allocator_allocate_pages(page_allocator_t *self, size_t num_pages) {
    // allocates contiguous pages, returns start address or zero.
    // initializes usage counter to 1
    allocator_priv_data_t *data = (allocator_priv_data_t *)self->private_data;
    uint32_t base_page = 0;

    for (size_t p = data->next_allocation_hint; p < data->num_pages - num_pages; p++) {
        if (consecutive_pages_free(data, p, num_pages)) {
            base_page = p;
            break;
        }
    }

    // wrap around, if needed
    if (base_page == 0) {
        for (size_t p = 1; p < data->next_allocation_hint; p++) {
            if (consecutive_pages_free(data, p, num_pages)) {
                base_page = p;
                break;
            }
        }
    }

    if (base_page != 0) {
        set_consecutive_pages_value(data, base_page, num_pages, 1);
        data->next_allocation_hint = base_page + num_pages;
        return PAGE_NO_TO_ADDR(base_page);
    }

    // could not find anything
    return 0;
}

static error_t page_allocator_retain_page(page_allocator_t *self, phys_addr_t page_address) {
    // increases usage counter by 1, used by processes sharing memory page
    allocator_priv_data_t *data = (allocator_priv_data_t *)self->private_data;

    uint32_t page_no = ADDR_TO_PAGE_NO((uintptr_t)page_address);

    if (page_no >= data->num_pages) {
        log_error("asked to retain page %u, while total is %u", page_no, data->num_pages);
        return ERR_BAD_ARGUMENT;
    }
    
    if (data->page_refs[page_no] == 0xFFFF) {
        log_error("asked to retain page %u, which is already at max value", page_no);
        return ERR_OVERFLOWN;
    }

    if (data->page_refs[page_no] == 0) {
        log_error("asked to retain page %u, which is not allocated", page_no);
        return ERR_BAD_ARGUMENT;
    }

    // no change to free pages
    data->page_refs[page_no] += 1;
    return OK;
}

static error_t page_allocator_release_page(page_allocator_t *self, phys_addr_t page_address) {
    // decreases counter by 1, if zero, page is back to allocatable
    allocator_priv_data_t *data = (allocator_priv_data_t *)self->private_data;

    uint32_t page_no = ADDR_TO_PAGE_NO((uintptr_t)page_address);
    if (page_no >= data->num_pages) {
        log_error("asked to release page %u, while total is %u", page_no, data->num_pages);
        return ERR_BAD_ARGUMENT;
    }
    
    if (data->page_refs[page_no] == 0) {
        log_error("asked to release page %u, which is already at zero", page_no);
        return ERR_UNDERFLOW;
    }

    data->page_refs[page_no] -= 1;
    return OK;
}

static struct allocator_priv_data allocator_data;

static page_allocator_t static_page_allocator = {
    .allocate_pages = page_allocator_allocate_pages,
    .retain_page = page_allocator_retain_page,
    .release_page = page_allocator_release_page,
    .private_data = &allocator_data
};

// --------------------------------------------------------------

page_allocator_t *create_page_allocator(
    uint64_t highest_memory_address,    // how much memory do we have? supporting up to 4GB
    region_t allocator_workspace,       // inside kernel, predefined 2MB for counters, later identity mapped
    region_t *available_regions,        // from BIOS discovery
    size_t num_available_regions,
    region_t *reserved_regions,         // kernel, etc
    size_t num_reserved_regions
) {
    allocator_priv_data_t *data = &allocator_data;

    data->highest_memory_address = highest_memory_address;
    data->page_refs = (void *)allocator_workspace.base;
    data->num_pages = ADDR_TO_PAGE_NO(data->highest_memory_address);
    
    // ensure workspace area has enough space
    uint32_t workspace_needed = data->num_pages * sizeof(uint16_t);
    if (allocator_workspace.size < workspace_needed)
        panic("page allocator tracks %lu pages, needs %lu bytes for counters, but %lu was given", data->num_pages, workspace_needed, allocator_workspace.size);
    
    // start with marking everying as unavailable
    for (size_t i = 0; i < data->num_pages; i++)
        data->page_refs[i] = RESERVED;

    // then mark available regions as available
    for (size_t i = 0; i < num_available_regions; i++)
        set_region_ref_value(data, available_regions[i], FREE);

    // then mark reserved regions as unavailable again
    for (size_t i = 0; i < num_reserved_regions; i++)
        set_region_ref_value(data, reserved_regions[i], RESERVED);

    // also ensure the counters area will not be allocatable
    set_region_ref_value(data, allocator_workspace, RESERVED);

    // and page 0 is the invalid page, so unavailable as well
    data->page_refs[0] = RESERVED;


    return &static_page_allocator;
}
