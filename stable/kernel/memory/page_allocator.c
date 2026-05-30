#include "page_allocator.h"
#include "../logger/logger.h"
#include "../utils/panic.h"
#include "../utils/assert.h"  // Added for unit tests
#include "../klib/string.h" // Added for unit tests (memset, memcpy)
#include "../memory/kheap.h" // Added for kmalloc/kfree

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
    if (base_page == 0) { // If not found from hint, try from beginning up to hint
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
        if (data->next_allocation_hint >= data->num_pages) {
            data->next_allocation_hint = 1; // Wrap around to start if end reached
        }
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
        log_error("asked to retain page %u, which is already at max value (RESERVED)", page_no);
        return ERR_OVERFLOWN; // Or ERR_BAD_ARGUMENT if we consider RESERVED as not allocatable
    }

    if (data->page_refs[page_no] == 0) {
        log_error("asked to retain page %u, which is not allocated (FREE)", page_no);
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
        log_error("asked to release page %u, which is already at zero (FREE)", page_no);
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


// -----------------------------------------------------------------------------------

#ifdef ENABLE_UNIT_TESTS

// Mock workspace for the page allocator
#define MOCK_WORKSPACE_SIZE (2 * 1024 * 1024) // 2MB as mentioned in create_page_allocator
// Removed: static uint8_t test_mock_workspace[MOCK_WORKSPACE_SIZE]; // This was the problematic static global array

// Helper to get internal private_data for assertions
static allocator_priv_data_t *get_priv_data(page_allocator_t *pa) {
    return (allocator_priv_data_t *)pa->private_data;
}

// Helper to create and initialize the page allocator for tests
// Now takes an output pointer for the dynamically allocated mock buffer
static page_allocator_t *create_test_allocator(
    void **out_mock_buffer, // Pointer to store the kmalloc'd buffer
    uint64_t highest_memory_address,
    region_t *available_regions,
    size_t num_available_regions,
    region_t *reserved_regions,
    size_t num_reserved_regions
) {
    // Allocate the mock workspace dynamically
    *out_mock_buffer = kmalloc(MOCK_WORKSPACE_SIZE);
    ASSERT(*out_mock_buffer != NULL);
    // Clear the mock workspace before each test
    memset(*out_mock_buffer, 0, MOCK_WORKSPACE_SIZE);
    
    region_t allocator_workspace = { .base = (uintptr_t)*out_mock_buffer, .size = MOCK_WORKSPACE_SIZE };
    page_allocator_t *pa = create_page_allocator(
        highest_memory_address,
        allocator_workspace,
        available_regions,
        num_available_regions,
        reserved_regions,
        num_reserved_regions
    );
    // As per the implementation, create_page_allocator returns a static instance
    // so we don't need to assert pa != NULL, but rather its private_data has been initialized.
    ASSERT(get_priv_data(pa)->page_refs == (uint16_t *)*out_mock_buffer); // Verify initialization
    return pa;
}

// New helper to destroy the test allocator's mock workspace
static void destroy_test_allocator(void *mock_buffer) {
    kfree(mock_buffer);
}


// Test case 1: Initialization
static void test_initialization() {
    log_info("Running test_initialization...");

    void *mock_buffer; // Declare buffer for dynamic allocation

    // Test with a simple setup
    uint64_t highest_mem = 4 * 1024 * 1024; // 4MB
    region_t available[] = {
        { .base = 0x1000, .size = 0x3000 }, // 3 pages
        { .base = 0x8000, .size = 0x4000 }  // 4 pages
    };
    region_t reserved[] = {
        { .base = 0x2000, .size = 0x1000 }  // 1 page within available
    };

    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 2, reserved, 1);
    allocator_priv_data_t *data = get_priv_data(pa);

    ASSERT(data->highest_memory_address == highest_mem);
    ASSERT(data->num_pages == ADDR_TO_PAGE_NO(highest_mem)); // Should be 1024 pages for 4MB
    ASSERT(data->page_refs == (uint16_t *)mock_buffer); // Using dynamically allocated mock workspace
    ASSERT(data->next_allocation_hint == 0);

    // Verify page_refs status
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x0000)] == RESERVED); // Page 0 is reserved
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x2000)] == RESERVED); // Reserved within available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x3000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x4000)] == RESERVED); // After available[0], up to available[1]
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x8000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x9000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0xA000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0xB000)] == FREE);     // Available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0xC000)] == RESERVED); // After available[1]

    destroy_test_allocator(mock_buffer);
    log_info("test_initialization PASSED.");
}

// Test case 2: Allocate single page
static void test_allocate_single_page() {
    log_info("Running test_allocate_single_page...");

    void *mock_buffer;
    uint64_t highest_mem = 4 * 1024 * 1024; // 4MB
    region_t available[] = {
        { .base = 0x1000, .size = 0x1000 }, // 1 page
        { .base = 0x3000, .size = 0x2000 }  // 2 pages
    };
    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 2, NULL, 0);
    allocator_priv_data_t *data = get_priv_data(pa);

    phys_addr_t addr1 = pa->allocate_pages(pa, 1);
    ASSERT(addr1 == 0x1000);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 1);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x1000) + 1);

    phys_addr_t addr2 = pa->allocate_pages(pa, 1);
    ASSERT(addr2 == 0x3000); // Should skip 0x2000 as it's not marked available
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x3000)] == 1);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x3000) + 1);

    destroy_test_allocator(mock_buffer);
    log_info("test_allocate_single_page PASSED.");
}

// Test case 3: Allocate multiple pages
static void test_allocate_multiple_pages() {
    log_info("Running test_allocate_multiple_pages...");

    void *mock_buffer;
    uint64_t highest_mem = 4 * 1024 * 1024; // 4MB
    region_t available[] = {
        { .base = 0x1000, .size = 0x8000 } // 8 pages
    };
    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 1, NULL, 0);
    allocator_priv_data_t *data = get_priv_data(pa);

    // Allocate 3 contiguous pages
    phys_addr_t addr = pa->allocate_pages(pa, 3);
    ASSERT(addr == 0x1000);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 1);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x2000)] == 1);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x3000)] == 1);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x3000) + 1);

    // Allocate another 2 contiguous pages
    addr = pa->allocate_pages(pa, 2);
    ASSERT(addr == 0x4000);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x4000)] == 1);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x5000)] == 1);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x5000) + 1);

    // Try to allocate 4 pages (should fail as only 1 contiguous page left)
    addr = pa->allocate_pages(pa, 4);
    ASSERT(addr == 0);

    destroy_test_allocator(mock_buffer);
    log_info("test_allocate_multiple_pages PASSED.");
}

// Test case 4: Retain and Release pages
static void test_retain_release_pages() {
    log_info("Running test_retain_release_pages...");

    void *mock_buffer;
    uint64_t highest_mem = 4 * 1024 * 1024; // 4MB
    region_t available[] = {
        { .base = 0x1000, .size = 0x1000 } // 1 page
    };
    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 1, NULL, 0);
    allocator_priv_data_t *data = get_priv_data(pa);

    phys_addr_t addr = pa->allocate_pages(pa, 1);
    ASSERT(addr == 0x1000);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 1);

    // Retain page
    ASSERT(pa->retain_page(pa, addr) == OK);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 2);

    // Retain again
    ASSERT(pa->retain_page(pa, addr) == OK);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 3);

    // Release page
    ASSERT(pa->release_page(pa, addr) == OK);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 2);

    // Release again (should still be allocated)
    ASSERT(pa->release_page(pa, addr) == OK);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == 1);

    // Release one more time (should become free)
    ASSERT(pa->release_page(pa, addr) == OK);
    ASSERT(data->page_refs[ADDR_TO_PAGE_NO(0x1000)] == FREE);

    // Attempt to release an already free page
    ASSERT(pa->release_page(pa, addr) == ERR_UNDERFLOW);

    // Attempt to retain a reserved page (page 0)
    ASSERT(pa->retain_page(pa, 0) == ERR_BAD_ARGUMENT);

    // Attempt to release an invalid page address (out of bounds)
    ASSERT(pa->release_page(pa, PAGE_NO_TO_ADDR(data->num_pages + 1)) == ERR_BAD_ARGUMENT);

    destroy_test_allocator(mock_buffer);
    log_info("test_retain_release_pages PASSED.");
}

// Test case 5: Allocation Failure and Exhaustion
static void test_allocation_failure_exhaustion() {
    log_info("Running test_allocation_failure_exhaustion...");

    void *mock_buffer;
    uint64_t highest_mem = 0x3000; // 3 pages total (page 0 reserved)
    region_t available[] = {
        { .base = 0x1000, .size = 0x1000 }, // 1 page
        { .base = 0x2000, .size = 0x1000 }  // 1 page
    };
    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 2, NULL, 0);
    allocator_priv_data_t *data = get_priv_data(pa);

    // Allocate first page
    phys_addr_t addr1 = pa->allocate_pages(pa, 1);
    ASSERT(addr1 == 0x1000);

    // Allocate second page
    phys_addr_t addr2 = pa->allocate_pages(pa, 1);
    ASSERT(addr2 == 0x2000);

    // Try to allocate one more page (should fail, no more free pages)
    phys_addr_t addr3 = pa->allocate_pages(pa, 1);
    ASSERT(addr3 == 0);

    // Release addr1, then try to allocate
    pa->release_page(pa, addr1);
    addr3 = pa->allocate_pages(pa, 1);
    ASSERT(addr3 == addr1); // Should re-allocate the freed page

    // Release addr2
    pa->release_page(pa, addr2);

    // Allocate 2 contiguous pages (should fail as only 2 single pages are free)
    addr3 = pa->allocate_pages(pa, 2);
    ASSERT(addr3 == 0);
    
    destroy_test_allocator(mock_buffer);
    log_info("test_allocation_failure_exhaustion PASSED.");
}

// Test case 6: Wraparound and Hint behavior
static void test_wraparound_hint() {
    log_info("Running test_wraparound_hint...");

    void *mock_buffer;
    uint64_t highest_mem = 0x8000; // 8 pages
    region_t available[] = {
        { .base = 0x1000, .size = 0x7000 } // 7 pages (0x1000 to 0x7000)
    };
    page_allocator_t *pa = create_test_allocator(&mock_buffer, highest_mem, available, 1, NULL, 0);
    allocator_priv_data_t *data = get_priv_data(pa);

    // Allocate some pages at the start
    pa->allocate_pages(pa, 2); // 0x1000, 0x2000 allocated
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x2000) + 1);

    // Reserve pages in the middle
    data->page_refs[ADDR_TO_PAGE_NO(0x4000)] = RESERVED;
    data->page_refs[ADDR_TO_PAGE_NO(0x5000)] = RESERVED;

    // Allocate 1 page. Hint is at 0x3000 (page 3). Should find 0x3000.
    phys_addr_t addr = pa->allocate_pages(pa, 1);
    ASSERT(addr == 0x3000);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x3000) + 1);

    // Now all pages from 0x1000 to 0x3000 are allocated.
    // 0x4000, 0x5000 are reserved.
    // Free pages are 0x6000, 0x7000.
    // Hint is after 0x3000. It should find 0x6000.
    addr = pa->allocate_pages(pa, 1);
    ASSERT(addr == 0x6000);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x6000) + 1);

    // Now release 0x1000. Hint is past it. Should wraparound and find 0x1000.
    pa->release_page(pa, 0x1000);
    addr = pa->allocate_pages(pa, 1);
    ASSERT(addr == 0x7000); // 0x7000 is next free from hint.
                            // The logic for find_next_free in page_allocator.c
                            // iterates from next_allocation_hint first, then wraps around.
                            // So it will find 0x7000 first, then 0x1000.
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x7000) + 1);

    // Now all pages from 0x1000 to 0x3000 are allocated (except 0x1000 just freed).
    // All other available pages are now allocated (0x2000, 0x3000, 0x6000, 0x7000)
    // Try to allocate 1 page, should find 0x1000 via wraparound.
    addr = pa->allocate_pages(pa, 1);
    ASSERT(addr == 0x1000);
    ASSERT(data->next_allocation_hint == ADDR_TO_PAGE_NO(0x1000) + 1);
    
    destroy_test_allocator(mock_buffer);
    log_info("test_wraparound_hint PASSED.");
}

void page_allocator_unit_tests() {
    log_info("Running page_allocator unit tests...");

    test_initialization();
    test_allocate_single_page();
    test_allocate_multiple_pages();
    test_retain_release_pages();
    test_allocation_failure_exhaustion();
    test_wraparound_hint();

    log_info("All page_allocator unit tests completed.");
}

#endif // ENABLE_UNIT_TESTS
