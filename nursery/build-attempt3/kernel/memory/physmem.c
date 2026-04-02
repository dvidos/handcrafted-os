#include "../utils/panic.h"
#include "../arch/cpu.h"
#include "../logger/logger.h"
#include "physmem.h"

// 4GB of memory / 4K page size --> 1M pages
// 1M pages / 32 bits in an uint32_t  => 32K uint32_t numbers.
// 4K page size / 4 bytes that a uint32_t takes -> 1K uint32_t per page
// so, using 32 pages, we can have a "used" bit for each page in memory
// the initialization process is:
// - mark everything used.
// - mark availabile memory (e820) as free
// - mark things we know (kernel, kheap, the used pages bitmap etc) as used
// - mark the pages of the bitmap as used

MODULE("PMM2", LOG_LEVEL_WARN);

#define PAGE_SIZE   4096

struct pmm_data_t {
    
    // we need to know the address of the 32 pages that contain the "used" bitmap
    // 32K uint32_t long, or 32 pages of 4K each
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t *bitmap; 
    uint32_t bitmap_uint_count;
    uint32_t next_allocation_page_hint;
    pmm_allocator_t allocator;
};

static struct pmm_data_t pmm_data;

static inline uint32_t round_up_4k(uint32_t address) { return ((address + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE; }
static inline uint32_t round_down_4k(uint32_t address) { return (address / PAGE_SIZE) * PAGE_SIZE; }

static inline uint32_t page_no_for_address(phys_addr_t address) {
    return address / PAGE_SIZE;
}

static inline phys_addr_t address_of_page(uint32_t page_no) {
    return page_no * PAGE_SIZE;
}

static inline bool is_page_used(uint32_t page_no) {
    return pmm_data.bitmap[page_no / 32] & ((uint32_t)1 << (page_no % 32));
}

static inline bool is_page_free(uint32_t page_no) {
    return !is_page_used(page_no);
}

static inline void mark_page_used(uint32_t page_no) {
    if (is_page_used(page_no))
        return;
    pmm_data.bitmap[page_no / 32] |= ((uint32_t)1 << (page_no % 32));
    pmm_data.free_pages--;
}

static inline void mark_page_free(uint32_t page_no) {
    if (is_page_used(page_no)) {
        pmm_data.bitmap[page_no / 32] &= ~((uint32_t)1 << (page_no % 32));
        pmm_data.free_pages++;
    }
}

void pmm_initialize(uint64_t highest_machine_address, phys_addr_t bitmap_address, size_t bitmap_size_bytes) {
    pmm_data.allocator.allocate_physical_page = pmm_allocate_physical_page;
    pmm_data.allocator.free_physical_page = pmm_free_physical_page;

    // this phys mem manager supports up to 4 GB of physical memory. 
    if (highest_machine_address > 4 * GB)
        highest_machine_address = 0xFFFFFFFF;

    // 4GB of memory would give us 1M of pages, 32k integers of 32 bit, taking 128 KB bytes
    if (bitmap_size_bytes < 128 * KB)
        panic("pmm needs at least 128 KB for bitmap of 4GB");

    pmm_data.total_pages = (uint32_t)((highest_machine_address + 4095) / PAGE_SIZE);
    pmm_data.bitmap = (uint32_t *)bitmap_address;
    pmm_data.bitmap_uint_count = (pmm_data.total_pages + 31) / 32;
    pmm_data.next_allocation_page_hint = 0;

    // initially mark everything as unusable
    for (uint32_t i = 0; i < pmm_data.bitmap_uint_count; i++)
        pmm_data.bitmap[i] = 0xFFFFFFFF; // used, or unusable
    
    pmm_data.free_pages = 0;
}

void pmm_mark_region_available(phys_addr_t start, size_t length) {
    if (length == 0)
        return;
    
    start = round_down_4k(start);
    length = round_up_4k(length);
    uint64_t end = start + length;
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE)
        mark_page_free(page_no_for_address(addr));
}

void pmm_mark_region_reserved(phys_addr_t start, size_t length) {
    if (length == 0)
        return;
    
    start = round_down_4k(start);
    length = round_up_4k(length);
    uint64_t end = start + length;
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE)
        mark_page_used(page_no_for_address(addr));
}

void pmm_finish_initialization() {
    // mark the pages of the bitmap as reserved
    phys_addr_t bitmap_start = (phys_addr_t)pmm_data.bitmap;
    phys_addr_t bitmap_end = (phys_addr_t)(pmm_data.bitmap + pmm_data.bitmap_uint_count);
    for (phys_addr_t addr = bitmap_start; addr < bitmap_end; addr += PAGE_SIZE)
        mark_page_used(page_no_for_address(addr));

    // page zero is invalid, null pointers also.
    mark_page_used(0);
}

phys_addr_t _find_next_free_page() {
    uint32_t index = pmm_data.next_allocation_page_hint / 32;
    for (uint32_t times = 0; times < pmm_data.bitmap_uint_count; times++) {
        if (pmm_data.bitmap[index] == 0xFFFFFFFF) {
            if (++index >= pmm_data.bitmap_uint_count)
                index = 0;
            continue;
        }

        for (int bit = 0; bit < 32; bit++) {
            uint32_t page_no = index * 32 + bit;
            if (page_no >= pmm_data.total_pages)
                continue; // last bits of the last uint32 may not participate

            if (pmm_data.bitmap[index] & (uint32_t)1 << bit)
                continue;
            
            pmm_data.next_allocation_page_hint = (page_no + 1) % pmm_data.total_pages;
            return page_no;
        }

        if (++index >= pmm_data.bitmap_uint_count)
            index = 0;
    }

    return INVALID_PAGE;
}

phys_addr_t pmm_allocate_physical_page() { 
    phys_addr_t addr = 0;
    
    pushcli();
    int page_no = _find_next_free_page();
    if (page_no != INVALID_PAGE) {
        mark_page_used(page_no);
        addr = address_of_page(page_no);
    }
    popcli();

    return addr;
}

bool pmm_is_page_used(phys_addr_t addr) { 
    if (addr != round_down_4k(addr))
        panic("Checking physical address not aligned to 4k (0x%x)", addr);
    uint32_t page_no = page_no_for_address(addr);
    if (page_no >= pmm_data.total_pages)
        panic("Freeing page outside of total pages (page_no=%u)", page_no);
    return is_page_used(page_no);
}

bool pmm_is_page_free(phys_addr_t addr) { 
    return !pmm_is_page_used(addr);
}

void pmm_free_physical_page(phys_addr_t addr) { 
    if (addr != round_down_4k(addr))
        panic("Freeing physical address not aligned to 4k (0x%x)", addr);
    uint32_t page_no = page_no_for_address(addr);
    if (page_no >= pmm_data.total_pages)
        panic("Freeing page outside of total pages (page_no=%u)", page_no);
    if (!is_page_used(page_no))
        panic("Attempt to free a non-allocated page (page_no=%u, addr=0x%08x)", page_no, addr);
    if (addr >= (phys_addr_t)pmm_data.bitmap && addr < (phys_addr_t)(pmm_data.bitmap + pmm_data.bitmap_uint_count))
        panic("Attempt to free page in the used pages bitmap");
    
    pushcli();
    mark_page_free(page_no_for_address(addr));
    popcli();
}

phys_addr_t pmm_allocate_consecutive_pages(size_t total_bytes) {
    if (total_bytes <= PAGE_SIZE)
        return pmm_allocate_physical_page();

    pushcli();

    phys_addr_t addr = 0;
    int pages_needed = round_up_4k(total_bytes) / PAGE_SIZE;
    uint32_t starting_page = pmm_data.next_allocation_page_hint;
    uint32_t iterations_left = pmm_data.total_pages;

    while (iterations_left-- > 0) {
        uint32_t first_page = _find_next_free_page();
        if (first_page == INVALID_PAGE)
            break;
        if (first_page + pages_needed > pmm_data.total_pages)
            continue; // if block does not fit, continue till we wrap around

        bool all_consecutive_free = true;
        for (int extra = 1; extra < pages_needed; extra++) {
            if (!is_page_free(first_page + extra)) {
                all_consecutive_free = false;
                break;
            }
        }

        if (all_consecutive_free) {
            for (int extra = 0; extra < pages_needed; extra++)
                mark_page_used(first_page + extra);
            addr = address_of_page(first_page);
            break;
        }
    }

    popcli();
    return addr;
}

void pmm_free_consecutive_pages(phys_addr_t address, size_t total_bytes) {
    if (total_bytes <= PAGE_SIZE)
        pmm_free_physical_page(address);

    pushcli();

    int total_pages = round_up_4k(total_bytes) / PAGE_SIZE;
    uint32_t first_page = page_no_for_address(address);
    for (int extra = 0; extra < total_pages; extra++)
        mark_page_free(first_page + extra);

    popcli();
}

uint32_t pmm_total_pages() {
    return pmm_data.total_pages;
}

uint32_t pmm_free_pages() {
    return pmm_data.free_pages;
}

uint32_t pmm_used_pages() {
    return pmm_data.total_pages - pmm_data.free_pages;
}

pmm_allocator_t pmm_get_pmm_allocator() {
    return pmm_data.allocator;
}

void pmm_debug_bitmap_ranges() {
    uint32_t start_page = 0;
    bool current_used = (pmm_data.bitmap[0] & 1) != 0; // first page
    
    log_info("Physical memory manager bitmap ranges");
    for (uint32_t i = 1; i < pmm_data.total_pages; i++) {
        uint32_t word = pmm_data.bitmap[i / 32];
        bool used = (word >> (i % 32)) & 1;

        if (used != current_used) {
            // Print the previous range
            uint64_t start_addr = (uint64_t)start_page * PAGE_SIZE;
            uint64_t end_addr = (uint64_t)i * PAGE_SIZE;
            log_info("  %s: 0x%08llx - 0x%08llx, %u KB or %u MB",
                   current_used ? "used" : "free",
                   start_addr,
                   end_addr - 1, 
                   (uint32_t)((end_addr - start_addr) / 1024),
                   (uint32_t)((end_addr - start_addr) / (1024 * 1024))
                );

            // Start new range
            start_page = i;
            current_used = used;
        }
    }

    // Print the last range
    uint64_t start_addr = (uint64_t)start_page * PAGE_SIZE;
    uint64_t end_addr = (uint64_t)pmm_data.total_pages * PAGE_SIZE;
    log_info("  %s: 0x%08llx - 0x%08llx, %u KB or %u MB",
            current_used ? "used" : "free",
            start_addr,
            end_addr - 1, 
            (uint32_t)((end_addr - start_addr) / 1024),
            (uint32_t)((end_addr - start_addr) / (1024 * 1024))
        );
}
