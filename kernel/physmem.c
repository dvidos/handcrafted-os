#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "string.h"
#include "cpu.h"
#include "klog.h"
#include "multiboot.h"



// inspiration from here: http://www.brokenthorn.com/Resources/OSDev17.html
// to map 4BGB of data, using 4KB pages, we need 1M bits.
// if a uint32_t can hold 32 bits, we need 32768 of them.
// that would take 128KB of memory, or 32 4K-pages.


// exported methods:
void init_physical_memory_manager(multiboot_info_t *info);
void *allocate_physical_page();
void *allocate_consecutive_physical_pages(size_t size_in_bytes);
void free_physical_page(void *address);
void dump_physical_memory_map();


// static, for this file onle
#define PAGE_SIZE 4096
#define ONE_MB    0x100000
#define ONE_GB    0x40000000
uint32_t page_status_bitmaps[32768];
uint32_t highest_memory_address;
uint32_t total_free_pages;
uint32_t total_used_pages;

static void mark_physical_memory_available(void *address, size_t size);
static void mark_physical_memory_unavailable(void *address, size_t size);
static bool page_is_used(int page_no);
static bool page_is_free(int page_no);
static void mark_page_used(int page_no);
static void mark_page_free(int page_no);
static uint32_t round_up_4k(uint32_t number);
static uint32_t round_down_4k(uint32_t number);
static void *page_num_to_address(int page_num);
static uint32_t address_to_page_num(void *address);



void init_physical_memory_manager(multiboot_info_t *info) {
    // mark all of it as used, will open up the available physical_memory_upper_limit
    memset((char *)page_status_bitmaps, 0xFF, sizeof(page_status_bitmaps));
    highest_memory_address = 0;
    total_free_pages = 0;
    total_used_pages = 0;

    // one way of doing this, is the simple mem members:
    if (info->flags & MULTIBOOT_INFO_MEM_MAP) {
        int map_size = info->mmap_length;
        multiboot_memory_map_t *map_entry = (multiboot_memory_map_t *)info->mmap_addr;
        while (map_size > 0) {
klog("mapsize %d\n", map_size);
            // for now we only care for available memory, up to 4GB
            if (map_entry->type == MULTIBOOT_MEMORY_AVAILABLE
                && map_entry->addr < 0xFFFFFFFF) {
                uint32_t addr32 = (uint32_t)(map_entry->addr & 0xFFFFFFFF);
                uint32_t len32 = (uint32_t)(map_entry->len & 0xFFFFFFFF);
                mark_physical_memory_available((void *)addr32, len32);
                if (addr32 + len32 > highest_memory_address)
                    highest_memory_address = addr32 + len32;
            }
            map_size -= sizeof(multiboot_memory_map_t);
            map_entry++;
        }
    } else if (info->flags & MULTIBOOT_MEMORY_INFO) {
        mark_physical_memory_available((void *)0, info->mem_lower * 1024);
        // upper memory is supposed to be the first upper memory hole minus 1MB
        mark_physical_memory_available((void *)ONE_MB, (info->mem_upper * 1024) - ONE_MB);
        highest_memory_address = info->mem_upper * 1024 - 1;
    } else {
        // aim for 1 GB and hope for the best... :-(
        mark_physical_memory_available((void *)0, 512 * 1024);
        mark_physical_memory_available((void *)ONE_MB, ONE_GB - ONE_MB);
        highest_memory_address = ONE_GB - 1;
    }

    // the very first page is unusable, to allow NULL pointers to be invalid
    mark_page_used(0); 
}

static void mark_physical_memory_available(void *address, size_t size) {
    klog("marking as available, p=%p, len=%u", address, size);
    int first_page_no = address_to_page_num((void *)round_up_4k((uint32_t)address));
    int num_of_pages = round_down_4k(size) / PAGE_SIZE;
    for (int i = 0; i < num_of_pages; i++)
        mark_page_free(first_page_no + i);
    total_free_pages += num_of_pages;
}

static void mark_physical_memory_unavailable(void *address, size_t size) {
    int first_page_no = address_to_page_num((void *)round_down_4k((uint32_t)address));
    int num_of_pages = round_up_4k(size) / PAGE_SIZE;
    for (int i = 0; i < num_of_pages; i++)
        mark_page_used(first_page_no + i);
    total_used_pages += num_of_pages;
}

static inline bool page_is_used(int page_no) {
    return page_status_bitmaps[page_no / 32] & (1 << (page_no % 32));
}

static inline bool page_is_free(int page_no) {
    return (page_status_bitmaps[page_no / 32] & (1 << (page_no % 32))) == 0;
}

static inline void mark_page_used(int page_no) {
    page_status_bitmaps[page_no / 32] |= (1 << (page_no % 32));
}

static inline void mark_page_free(int page_no) {
    page_status_bitmaps[page_no / 32] &= ~(1 << (page_no % 32));
}

static inline uint32_t round_up_4k(uint32_t number) {
    return (number + 4095) & 0xFFFFF000;
}

static inline uint32_t round_down_4k(uint32_t number) {
    return number & 0xFFFFF000;
}

static inline void *page_num_to_address(int page_num) {
    return (void *)(page_num << 12);
}

static inline uint32_t address_to_page_num(void *address) {
    return ((uint32_t)address >> 12);
}

static int find_first_free_physical_page_no(int min_page_no) {
    int starting_i = min_page_no / 32;
    for (int i = starting_i; i < (int)(sizeof(page_status_bitmaps) / sizeof(page_status_bitmaps[0])); i++) {
        if (page_status_bitmaps[i] == 0xFFFFFFFF)
            continue;
        
        int starting_bit = (i == starting_i) ? min_page_no % 32 : 0;
        for (int bit = starting_bit; bit <= 32; bit++) {
            if ((page_status_bitmaps[i] & (1 << bit)) == 0) {
                return i * 32 + bit;
            }
        }
    }

    return -1;
}

void *allocate_physical_page() {
    pushcli();
    int page_no = find_first_free_physical_page_no(0);
    if (page_no == -1)
        panic("Cannot find free page to allocate");
    mark_page_used(page_no);
    total_free_pages--;
    total_used_pages++;
    popcli();
    return page_num_to_address(page_no);
}

void *allocate_consecutive_physical_pages(size_t size_in_bytes) {
    if (size_in_bytes <= PAGE_SIZE)
        return allocate_physical_page();
    
    pushcli();
    int pages_needed = round_up_4k(size_in_bytes) / PAGE_SIZE;
    int starting_page_no = 0;
    while (true) {
        int page_no = find_first_free_physical_page_no(starting_page_no);
        if (page_no == -1)
            panic("Cannot find free consecutive pages to allocate");
        bool all_needed_extra_pages_are_free = true;
        int extra_page = 0;
        for (extra_page = 1; extra_page < pages_needed; extra_page++) {
            if (page_is_used(page_no + extra_page)) {
                all_needed_extra_pages_are_free = false;
                break;
            }
        }
        if (all_needed_extra_pages_are_free)
            break;
        // otherwise, search another region
        starting_page_no = page_no + extra_page + 1;
    }

    for (int i = 0; i < pages_needed; i++) {
        mark_page_used(i);
        total_used_pages++;
        total_free_pages--;
    }
    popcli();
    return page_num_to_address(starting_page_no);
}

void free_physical_page(void *address) {
    pushcli();
    int page_no = address_to_page_num(address);
    if (!page_is_used(page_no))
        panic("Freeing unused memory page");
    mark_page_free(page_no);
    total_free_pages++;
    total_used_pages--;
    popcli();
}


void dump_physical_memory_map() {
    char line_symbols[64+1];

    // we have 32768 integers, each representing 32 pages.
    // 32 pages is 128 KB. We need 32 of those integers to represent 4MB
    // 64 4MB chars per line => 256 MB per line => 16 lines for 4GB
    // this allows for offset as well.
    
    // 0        1         2         3         4         5         6         7         8
    // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
    // ----------------------- Physical Memory Allocation Map -----------------------
    //   Offset  Status (.=empty, *=partial, U=unavailable) - 1 char = 4MB
    //     0 MB  ................................................................
    //   256 MB  ................................................................
    //   512 MB  ................................................................
    //   768 MB  ................................................................
    //  1024 MB  ................................................................
    //  1280 MB  ................................................................
    //  1536 MB  ................................................................
    //  1792 MB  ................................................................
    //  2048 MB  ................................................................
    //  2304 MB  ................................................................
    //  2560 MB  ................................................................
    //  2816 MB  ................................................................
    //  3072 MB  ................................................................
    //  3328 MB  ................................................................
    //  3584 MB  ................................................................
    //  3840 MB  ................................................................
 
    // each uint32_t bitmap represents 32 pages => 128KB
    // we need to collect 32 of those for one character
    klog("----------------------- Physical Memory Allocation Map -----------------------\n");
    klog("  Offset  Status (.=empty, *=partial, U=unavailable) - 1 char = 4MB\n");
    int index = 0;
    for (int line = 0; line < 16; line++) {
        memset(line_symbols, 0, sizeof(line_symbols));
        for (int char_block = 0; char_block < 64; char_block++) {
            bool free_page_found = false;
            bool used_page_found = false;
            for (int block_bitmap = 0; block_bitmap < 32; block_bitmap++) {
                if (page_status_bitmaps[index] != 0xFFFFFFFF)
                    free_page_found = true;
                if (page_status_bitmaps[index] != 0x00000000)
                    used_page_found = true;
                index++;
            }
            char symbol = !free_page_found ? 'U' : (!used_page_found ? '.' : '*');
            line_symbols[char_block] = symbol;
        }
        klog("  %4d MB  %s\n", line * 256, line_symbols);
    }
    klog("highest memory address 0x%x, %u used pages, %u free pages\n",
        highest_memory_address,
        total_used_pages,
        total_free_pages
    );
}