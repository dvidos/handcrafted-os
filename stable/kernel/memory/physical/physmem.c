#include "../../utils/panic.h"
#include "../../arch/cpu.h"
#include "../../logger/logger.h"
#include "physmem.h"


MODULE("PMM2", LOG_LEVEL_WARN);

#define PAGE_SIZE   4096


// bits 15, 14 are special, bits 13-0 used for counting
// alternatively, we could store the "is_system", "is_kernel" info elsewhere, in a bitmap
#define PAGE_SYSTEM_RESERVED    (uint16_t)((uint16_t)1 << 15)
#define PAGE_KERNEL_RESERVED    (uint16_t)((uint16_t)1 << 14)
#define PAGE_COUNTER_MASK       (uint16_t)((uint16_t)0x3FFF)   // 16383 processes to share the memory


struct pmm_data_t {
    
    uint32_t total_pages;
    uint32_t free_pages;

    uint16_t *page_data; // the new way of dealing with pages, using counters

    uint32_t last_allocated_page_hint;
    pmm_allocator_t allocator;
};

static struct pmm_data_t pmm_data;

static inline uint32_t round_up_4k(uint32_t address) { return ((address + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE; }
static inline uint32_t round_down_4k(uint32_t address) { return (address / PAGE_SIZE) * PAGE_SIZE; }

// -------------------------------------------------------------------------------

static inline uint32_t page_no_for_address(phys_addr_t address) {
    return address / PAGE_SIZE;
}

static inline phys_addr_t address_of_page(uint32_t page_no) {
    return page_no * PAGE_SIZE;
}

static inline bool is_page_system_reserved(uint32_t page_no) {
    return (pmm_data.page_data[page_no] & PAGE_SYSTEM_RESERVED);
}

static inline void set_page_system_reserved(uint32_t page_no) {
    pmm_data.page_data[page_no] |= PAGE_SYSTEM_RESERVED;
}

static inline void set_page_system_available(uint32_t page_no) {
    pmm_data.page_data[page_no] &= ~PAGE_SYSTEM_RESERVED;
}

static inline bool is_page_kernel_reserved(uint32_t page_no) {
    return (pmm_data.page_data[page_no] & PAGE_KERNEL_RESERVED);
}

static inline void set_page_kernel_reserved(uint32_t page_no) {
    pmm_data.page_data[page_no] |= PAGE_KERNEL_RESERVED;
}

static inline bool is_page_allocated(uint32_t page_no) {
    return (pmm_data.page_data[page_no] & PAGE_COUNTER_MASK) > 0;
}

static inline bool is_page_free(uint32_t page_no) {
    return !is_page_allocated(page_no);
}

static inline bool is_page_user_allocatable(uint32_t page_no) {
    return pmm_data.page_data[page_no] == 0; // must not have any bit turned on.
}

static inline uint16_t get_page_counter(uint32_t page_no) {
    return (pmm_data.page_data[page_no] & PAGE_COUNTER_MASK);
}

static inline void update_page_counter(uint32_t page_no, int delta) {
    register uint16_t pd = pmm_data.page_data[page_no];
    pd = (pd & ~PAGE_COUNTER_MASK) | ((pd & PAGE_COUNTER_MASK) + delta);
    pmm_data.page_data[page_no] = pd;
}

static inline void mark_page_used(uint32_t page_no) {
    if (is_page_allocated(page_no))
        return;
    update_page_counter(page_no, +1);
    pmm_data.free_pages--;
}

static inline void mark_page_free(uint32_t page_no) {
    if (is_page_free(page_no))
        return;
    update_page_counter(page_no, -1);
    pmm_data.free_pages++;
}

// --------------------------------------------------------------------------------

static size_t desired_work_area_size(uint64_t machine_max_memory_address) {
    // if we wanted to keep 2 bytes per page, with 2-3 bits for flags and the rest for counter
    // - RESERVED (e.g. kernel code pages cannot be reserved or released)
    
    size_t bytes_per_page = 2; // 2 flags and 14 bits of counter = 16383 procs
    uint32_t pages_to_track = (uint32_t)((machine_max_memory_address + (PAGE_SIZE-1)) / (uint64_t)PAGE_SIZE);
    size_t bytes_needed = pages_to_track * bytes_per_page;
    size_t pages_needed = round_up_4k(bytes_needed) / PAGE_SIZE;

    return pages_needed * PAGE_SIZE;
}

size_t pmm_get_desired_work_area_size(uint64_t machine_max_memory_address) {
    return desired_work_area_size(machine_max_memory_address);
}

void pmm_initialize(uint64_t highest_machine_address, phys_addr_t workarea_address, size_t workarea_bytes) {
    size_t bytes_needed = desired_work_area_size(highest_machine_address);
    if (workarea_bytes < bytes_needed)
        panic("PMM work area only %lu bytes, %lu needed", workarea_bytes, bytes_needed);
    
    log_info("Machine memory %lu MB / %lu pages, reserving %lu MB / %lu pages for physical page tracking",
        (uint32_t)(highest_machine_address / MB),
        (uint32_t)(highest_machine_address / PAGE_SIZE),
        bytes_needed / MB,
        bytes_needed / PAGE_SIZE
    );

    pmm_data.allocator.allocate_physical_page = pmm_allocate_physical_page;
    pmm_data.allocator.free_physical_page = pmm_free_physical_page;

    pmm_data.total_pages = (uint32_t)((highest_machine_address + (PAGE_SIZE-1)) / PAGE_SIZE);
    pmm_data.page_data = (uint16_t *)workarea_address;
    pmm_data.last_allocated_page_hint = 0;



    // initially mark everything as unusable
    for (uint32_t i = 0; i < pmm_data.total_pages; i++)
        set_page_system_reserved(i);
    
    pmm_data.free_pages = 0;
}

void pmm_mark_region_system_available(phys_addr_t start, size_t length) {
    if (length == 0)
        return;
    
    start = round_down_4k(start);
    length = round_up_4k(length);
    uint64_t end = start + length;
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t page = page_no_for_address(addr);
        set_page_system_available(page);
    }
}

void pmm_mark_region_kernel_reserved(phys_addr_t start, size_t length) {
    if (length == 0)
        return;
    
    start = round_down_4k(start);
    length = round_up_4k(length);
    uint64_t end = start + length;
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t page = page_no_for_address(addr);
        set_page_kernel_reserved(page);
    }
}

void pmm_finish_initialization() {
    // the pages of the PMM workarea are assumed to have been already reserved by the kernel

    // zero page invalid, similar to pointers at address zero (null)
    set_page_system_reserved(0);
}

static uint32_t _find_next_free_page_no() {
    uint32_t page_no = pmm_data.last_allocated_page_hint + 1;
    if (page_no >= pmm_data.total_pages)
        page_no = 0;

    for (uint32_t times = 0; times < pmm_data.total_pages; times++) {

        if (is_page_user_allocatable(page_no)) {
            pmm_data.last_allocated_page_hint = page_no;
            return page_no;
        }

        // try next page
        page_no++;
        if (page_no >= pmm_data.total_pages)
            page_no = 0;
    }

    return 0; // NULL / invalid
}

phys_addr_t pmm_allocate_physical_page() { 
    phys_addr_t addr = 0;
    
    int page_no = _find_next_free_page_no();
    if (page_no == 0)
        return 0; // not found


    mark_page_used(page_no);
    return address_of_page(page_no);
}

bool pmm_is_page_used(phys_addr_t addr) { 
    if (addr != round_down_4k(addr))
        panic("Checking physical address not aligned to 4k (0x%x)", addr);
    uint32_t page_no = page_no_for_address(addr);
    if (page_no >= pmm_data.total_pages)
        panic("Freeing page outside of total pages (page_no=%u)", page_no);
    
    return is_page_allocated(page_no);
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
    if (!is_page_allocated(page_no))
        panic("Attempt to free a non-allocated page (page_no=%u, addr=0x%08x)", page_no, addr);
    if (is_page_system_reserved(page_no))
        panic("Attempt to free a system reserved page (page_no=%u)", page_no);
    if (is_page_kernel_reserved(page_no))
        panic("Attempt to free a kernel reserved page (page_no=%u)", page_no);
    
    // for now, till we implement retain/release
    if (get_page_counter(page_no) != 1)
        panic("Attempt to free a page with counter unequal to 1 (page_no=%u)", page_no);

    mark_page_free(page_no);
}

phys_addr_t pmm_allocate_consecutive_pages(size_t total_bytes) {
    if (total_bytes <= PAGE_SIZE)
        return pmm_allocate_physical_page();

    phys_addr_t addr = 0;
    int pages_needed = round_up_4k(total_bytes) / PAGE_SIZE;
    uint32_t starting_page = pmm_data.last_allocated_page_hint;
    uint32_t iterations_left = pmm_data.total_pages;

    while (iterations_left-- > 0) {
        uint32_t first_page = _find_next_free_page_no();
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

    return addr;
}

void pmm_free_consecutive_pages(phys_addr_t address, size_t total_bytes) {
    if (total_bytes <= PAGE_SIZE)
        pmm_free_physical_page(address);

    int total_pages = round_up_4k(total_bytes) / PAGE_SIZE;
    uint32_t first_page = page_no_for_address(address);
    for (int extra = 0; extra < total_pages; extra++)
        mark_page_free(first_page + extra);

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

pmm_allocator_t *pmm_get_pmm_allocator() {
    return &pmm_data.allocator;
}

void pmm_data_formatter(log_write_stream_t *stream, va_list args) {
    // we'll use our own internal data

    stream->printf(stream, "Physical memory manager data");
    uint32_t sys_reserved = 0;
    uint32_t kern_reserved = 0;
    uint32_t user_allocated = 0;
    uint32_t user_allocatable = 0;
    for (uint32_t i = 0; i < pmm_data.total_pages; i++) {
        if (is_page_system_reserved(i))
            sys_reserved++;
        if (is_page_kernel_reserved(i))
            kern_reserved++;
        if (is_page_allocated(i))
            user_allocated++;
        if (is_page_user_allocatable(i))
            user_allocatable++;
    }

    // Status                   Pages        KB        MB
    // 12345678901234567890  12345678  12345678  12345678
    stream->printf(stream, "  Page Status              Pages        KB        MB");
    stream->printf(stream, "  %-20s  %8u  %8u  %8u", "System reserved", sys_reserved, sys_reserved * 4, (sys_reserved + 255) / 256);
    stream->printf(stream, "  %-20s  %8u  %8u  %8u", "Kernel reserved", kern_reserved, kern_reserved * 4, (kern_reserved + 255) / 256);
    stream->printf(stream, "  %-20s  %8u  %8u  %8u", "User allocated", user_allocated, user_allocated * 4, (user_allocated + 255) / 256);
    stream->printf(stream, "  %-20s  %8u  %8u  %8u", "User allocatable", user_allocatable, user_allocatable * 4, (user_allocatable + 255) / 256);
    stream->printf(stream, "  %-20s  %8u  %8u  %8u", "Total", pmm_data.total_pages, pmm_data.total_pages * 4, (pmm_data.total_pages + 255) / 256);
}
