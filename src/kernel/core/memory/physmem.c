#include <drivers/screen.h>
#include <klib/string.h>
#include <cpu.h>
#include <klog.h>
#include <multiboot.h>


// inspiration from here: http://www.brokenthorn.com/Resources/OSDev17.html
// to map 4BGB of data, using 4KB pages, we need 1M bits.
// if a uint32_t can hold 32 bits, we need 32768 of them.
// that would take 128KB of memory, or 32 4K-pages.



// static, for this file only
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


int physical_page_size() {
    return PAGE_SIZE;
}


void init_physical_memory_manager(multiboot_info_t *info, void *kernel_start_address, void *kernel_end_address) {
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

    // also, exclude the pages where the kernel is loaded
    size_t kernel_size = (size_t)kernel_end_address - (size_t)kernel_start_address;
    mark_physical_memory_unavailable((void *)kernel_start_address, kernel_size);
}

static void mark_physical_memory_available(void *address, size_t size) {
    klog_debug("marking as available, p=%p, len=%u", address, size);
    int first_page_no = address_to_page_num((void *)round_up_4k((uint32_t)address));
    int num_of_pages = round_down_4k(size) / PAGE_SIZE;
    for (int i = 0; i < num_of_pages; i++)
        mark_page_free(first_page_no + i);
    total_free_pages += num_of_pages;
}

static void mark_physical_memory_unavailable(void *address, size_t size) {
    klog_debug("marking as unavailable, p=%p, len=%u", address, size);
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

void *allocate_physical_page(void *minimum_address) {
    pushcli();
    int min_page_no = round_up_4k((uint32_t)minimum_address) / PAGE_SIZE;
    int page_no = find_first_free_physical_page_no(min_page_no);
    if (page_no == -1)
        panic("Cannot find free page to allocate");
    mark_page_used(page_no);
    total_free_pages--;
    total_used_pages++;
    popcli();
    void *ptr = page_num_to_address(page_no);
    klog_trace("allocate_physical_page() -> 0x%p (page_no %d)", ptr, page_no);
    return ptr;
}

void *allocate_consecutive_physical_pages(size_t size_in_bytes, void *minimum_address) {
    if (size_in_bytes <= PAGE_SIZE)
        return allocate_physical_page(minimum_address);
    
    pushcli();
    int pages_needed = round_up_4k(size_in_bytes) / PAGE_SIZE;
    int search_starting_page = round_up_4k((uint32_t)minimum_address) / PAGE_SIZE;
    int first_free_page_no = -1;
    while (true) {
        first_free_page_no = find_first_free_physical_page_no(search_starting_page);
        if (first_free_page_no == -1)
            panic("Cannot find free consecutive pages to allocate");
        bool all_needed_extra_pages_are_free = true;
        int extra_page = 0;
        for (extra_page = 1; extra_page < pages_needed; extra_page++) {
            if (page_is_used(first_free_page_no + extra_page)) {
                all_needed_extra_pages_are_free = false;
                break;
            }
        }
        if (all_needed_extra_pages_are_free)
            break;
        // otherwise, search another region
        search_starting_page = first_free_page_no + extra_page + 1;
    }

    for (int i = 0; i < pages_needed; i++) {
        mark_page_used(first_free_page_no + i);
        total_used_pages++;
        total_free_pages--;
    }
    popcli();
    void *ptr = page_num_to_address(first_free_page_no);
    klog_trace("allocate_consecutive_physical_pages() -> 0x%p (%d pages, first page is %d)", ptr, pages_needed, first_free_page_no);
    return ptr;
}

void free_physical_page(void *address) {
    klog_trace("free_physical_page(0x%p)", address);
    pushcli();
    int page_no = address_to_page_num(address);
    if (!page_is_used(page_no))
        panic("Freeing unused memory page");
    mark_page_free(page_no);
    total_free_pages++;
    total_used_pages--;
    popcli();
}

void free_consecutive_physical_pages(void *address, size_t size_in_bytes) {
    klog_trace("free_consecutive_physical_pages(0x%p, %u)", address, (uint32_t)size_in_bytes);
    int starting_page_no = address_to_page_num(address);
    int pages = round_up_4k(size_in_bytes) / PAGE_SIZE;
    for (int i = 0; i < pages; i++) {
        free_physical_page(page_num_to_address(starting_page_no + i));
    }
}

void dump_physical_memory_map_overall() {
    char line_buffer[64+1];

    // we have 32768 integers, each representing 32 pages.
    // 32 pages is 128 KB. We need 32 of those integers to represent 4MB
    // 64 4MB chars per line => 256 MB per line => 16 lines for 4GB
    // this allows for offset as well.
    
    // 0        1         2         3         4         5         6         7         8
    // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
    // ----------------------- Physical Memory Allocation Map -----------------------
    //     Offset  1 char = 4MB = x400000 = 1024 pages (.=free, *=partial, U=used)
    // 0x00000000  ................................................................
 
    // each uint32_t bitmap represents 32 pages => 128KB
    // we need to collect 32 of those for one character
    printf("----------------------- Physical Memory Allocation Map -----------------------\n");
    printf("    Offset  1 char = 4MB = x400000 = 1024 pages (.=free, *=partial, U=used)\n");
    int index = 0;
    for (uint32_t line = 0; line < 16; line++) {
        memset(line_buffer, 0, sizeof(line_buffer));
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
            line_buffer[char_block] = symbol;
        }
        printf("0x%08x  %s\n", (uint32_t)(line * 256 * 1024 * 1024), line_buffer);
    }
    printf("highest memory address 0x%x, %u used pages, %u free pages\n",
        highest_memory_address,
        total_used_pages,
        total_free_pages
    );
}

void dump_physical_memory_map_detail(uint32_t start_address) {
    char line_buffer[64+1];

    // we shall use one character per page, 64 pages per line, 
    // with 4K per page, each line is 256KB
    // in 16 lines we present 1024 pages, or 4MB worth of data.
    // this corresponds to one character in the overall memory map
    
    // 0        1         2         3         4         5         6         7         8
    // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
    // ----------------------- Detailed Memory Allocation Map -----------------------
    //     Offset  1 char = 4KB = 0x1000 = 1 page (.=free, U=unavailable)
    // 0x00000000  ................................................................
 
    start_address = round_down_4k(start_address);
    int page_no = address_to_page_num((void *)start_address);
    printf("----------------------- Detailed Memory Allocation Map -----------------------\n");
    printf("    Offset  1 char = 4KB = 0x1000 = 1 page (.=free, U=unavailable)\n");
    for (int line = 0; line < 16; line++) {
        memset(line_buffer, 0, sizeof(line_buffer));
        for (int c = 0; c < 64; c++) {
            line_buffer[c] = page_is_free(page_no) ? '.' : 'U';;
            page_no++;
        }
        printf("0x%08x  %s\n", start_address, line_buffer);
        start_address += (4096 * 64);
    }
}