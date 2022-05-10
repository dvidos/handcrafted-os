#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "multiboot.h"
#include "screen.h"
#include "string.h"



// /**
//  need to detect available memory
//  see also https://wiki.osdev.org/Detecting_Memory_(x86)
//  it seems people avoid very low memory (< 1MB)
//  there also seems to be a hole around 15M - 16M.

//  should be able to grab a page (?) of memory to give to a process
//  should be able to malloc/free at will

//  try to offer a multitude of sizes, in orders of 4x
//  from 1K, 4K, 16k, 64k, 256k, 1MB, 4MB, 16MB, 64MB, 256MB
//  on the other hand, try to avoid putting blocks in the memory area
//  to avoid fragments and easily collide free areas
//  good page: http://gee.cs.oswego.edu/dl/html/malloc.html

//  given a set of areas from kernel, we must work with whatever he gives us.

//  due to warnings of compiler, that pointers are 32 bits,
//  I decided to support only 32 bits address space.
//  if we need > 4GB, let's put in the effort.
// */

// #define min(a, b)   ((a) < (b) ? (a) : (b))
// #define max(a, b)   ((a) > (b) ? (a) : (b))
// #define MEM_MAGIC            0x4321 // something that fits in 14 bits
// #define ONE_MEGABYTE         0x100000 // 1mb
// #define LOW_MEMORY_SIZE      0xA0000  // 640k
// #define LOW_MEM_SAFE_START   0x4FF    // below that interrupts and bios data
// #define LOW_MEM_SAFE_TOP     0x7FFFF  // beyond that, video memory etc
// #define HIGH_MEM_SAFE_TOP    0xFEBFFFFF // about 4gb, above that various devices and dragons


// static void claim_usable_memory_area(uint32_t area_start, uint32_t area_end, uint32_t kernel_start, uint32_t kernel_end);
// static void consider_block_header(uint32_t start, uint32_t end);

// // simplest thing
// struct block_header {
//     uint32_t size;
//     uint16_t magic: 15;
//     uint16_t used: 1;
//     struct block_header *next;
// } __attribute__((packed));

// typedef struct block_header block_header_t;

// // easy to walk
// // allocate what's needed
// // consolidate adjascent free space

// static block_header_t *mem_bottom = NULL;


// void init_memory(unsigned int boot_magic, multiboot_info_t* mbi, uint32_t kernel_start, uint32_t kernel_end) {
//     if (boot_magic != 0x2BADB002)
//         return;

//     // buffer for errors
//     kernel_start -= 1024;
//     kernel_end += 1024;

//     // walk the memory map (carefully!)
//     int map_size = mbi->mmap_length;
//     multiboot_memory_map_t *entry = (multiboot_memory_map_t *)mbi->mmap_addr;
//     while (map_size > 0) {
//         if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
//             // just in case we boot into a computer that has > 4GB memory
//             // we currently support only 32 bit memory mapping
//             uint32_t area_addr = (uint32_t)min(entry->addr, (uint64_t)0xFFFFFFFF);
//             uint32_t area_end =  (uint32_t)min(entry->addr + entry->len, (uint64_t)0xFFFFFFFF);
//             claim_usable_memory_area(area_addr, area_end, kernel_start, kernel_end);
//         }
//         map_size -= sizeof(multiboot_memory_map_t);
//         entry++; // strongly typed pointer arithmetic
//     }
// }

// static void claim_usable_memory_area(uint32_t area_start, uint32_t area_end, uint32_t kernel_start, uint32_t kernel_end) {
//     // printf("-- claiming memory area 0x%x to 0x%x, len %i", area_start, area_end, area_end - area_start);
//     uint32_t start;
//     uint32_t end;

//     bool kernel_in_low_memory = kernel_end < LOW_MEMORY_SIZE;
//     bool kernel_above_1mb = kernel_end >= ONE_MEGABYTE;

//     // let's see if it is below 640k or above 1mb
//     if (area_end <= LOW_MEMORY_SIZE) {
//         // for low memory, we'll avoid first part up to 0x4FF,
//         // in case we want to return to real mode sometime.
//         // interrupt vectors and bios data area leave there
//         // on the high side, we will only go up to 0x7FFFF,
//         // as Extended Bios Data Area and video buffers exist there
//         // on high memory,

//         if (kernel_in_low_memory) {
//             // area below kernel
//             start = max(area_start, LOW_MEM_SAFE_START);
//             end = min(min(area_end, kernel_start), LOW_MEM_SAFE_TOP);
//             consider_block_header(start, end);

//             // area above kernel
//             start = max(max(area_start, kernel_end), LOW_MEM_SAFE_START);
//             end = min(area_end, LOW_MEM_SAFE_TOP);
//             consider_block_header(start, end);
//         } else {
//             // whole area, no kernel here
//             start = max(area_start, LOW_MEM_SAFE_START);
//             end = min(area_end, LOW_MEM_SAFE_TOP);
//             consider_block_header(start, end);
//         }

//     } else if (area_start >= ONE_MEGABYTE) {
//         // on high memory area, there may be a gap at 15 MB
//         // we'll support up to 4GB for now,
//         // there may be mapped devices higher than that
//         if (kernel_above_1mb) {
//             // area below kernel
//             start = max(area_start, ONE_MEGABYTE);
//             end = min(min(area_end, kernel_start), HIGH_MEM_SAFE_TOP);
//             consider_block_header(start, end);

//             // area above kernel
//             start = max(max(area_start, kernel_end), ONE_MEGABYTE);
//             end = min(area_end, HIGH_MEM_SAFE_TOP);
//             consider_block_header(start, end);
//         } else {
//             // whole area, no kernel here
//             start = area_start;
//             end = min(area_end, HIGH_MEM_SAFE_TOP);
//             consider_block_header(start, end);
//         }
//     }
// }

// static void consider_block_header(uint32_t start, uint32_t end) {
//     // printf("-- considering block 0x%x to 0x%x, len %i\n", start, end, end - start);
//     if (start > end || end - start < 4096)
//         return;

//     block_header_t *block = (block_header_t *)(uint32_t)start;
//     block->used = 0;
//     block->magic = MEM_MAGIC;
//     block->size = (end - start) - sizeof(block_header_t);
//     block->next = NULL;

//     if (mem_bottom == NULL) {
//         mem_bottom = block;
//     } else {
//         block_header_t *tail = mem_bottom;
//         while (tail->next != NULL) {
//             tail = tail->next;
//         }
//         tail->next = block;
//     }
// }

// void dump_heap() {
//     block_header_t *p = mem_bottom;
//     char *type;
//     printf("  Address          Length  Type\n");
//     //        0x00000000  00000000 KB  Used...
//     uint32_t free_mem = 0;
//     uint32_t used_mem = 0;
//     uint32_t free_blocks = 0;
//     uint32_t used_blocks = 0;

//     while (p != NULL) {
//         if (p->used) {
//             type = "Used";
//             used_mem += p->size;
//             used_blocks++;
//         } else {
//             type = "Free";
//             free_mem += p->size;
//             free_blocks++;
//         }
//         printf("  0x%08x  %8u KB  %s\n",
//             (uint32_t)p,
//             p->size / 1024,
//             type);
//         p = p->next;
//     }
//     free_mem /= 1024;
//     used_mem /= 1024;
//     printf("Total used memory %u KB (%u blocks)\n", (uint32_t)used_mem, used_blocks);
//     printf("Total free memory %u KB (%u blocks)\n", (uint32_t)free_mem, free_blocks);
// }

// /**
//  * Very simple strategy:
//  * walk the blocks, until we find a free, large enough
//  * split the block (create a new free block, return current block)
//  * this allocates lowest memory first
//  */
// void *malloc(size_t size) {

//     block_header_t *block = mem_bottom;
//     size = max(size, 1024);

//     while (block != NULL) {
//         if (block->used || block->size < size + sizeof(block_header_t)) {
//             block = block->next;
//             continue;
//         }

//         // it'd be useful to disable interrupts...
//         block_header_t *new_free_block = ((void *)block) + sizeof(block_header_t) + size;
//         new_free_block->size = block->size - size - sizeof(block_header_t);
//         new_free_block->used = 0;
//         new_free_block->magic = MEM_MAGIC;
//         new_free_block->next = block->next;

//         // existing block is now smaller, and will be used.
//         block->size = size;
//         block->used = 1;
//         block->next = new_free_block;

//         void *ptr = ((void *)block) + sizeof(block_header_t);
//         memset(ptr, 0, size);
//         return ptr;
//     }

//     return NULL;
// }

// /**
//  * Again simple logic
//  * check magic numbers,
//  * free the block,
//  * consolidate with next and prev, if applicable
//  */
// void free(void *address) {
//     // we need to walk the heap (performance is left for the future),
//     // find the address, mark it as free, find next, check magic, consolidate if free
//     block_header_t *block = ((void *)address) - sizeof(block_header_t);
//     if (block->magic != MEM_MAGIC) {
//         printf("Block freed at %p has corrupted magic number!\n", block);
//         // should terminate current process, but not kernel
//     }
//     block->used = 0; // easiest operation ever!

//     if (block->next != NULL) {
//         if (block->next->magic != MEM_MAGIC) {
//             printf("Block after freed one at %p has corrupted magic number!\n", block);
//         }

//         // see if we can consolidate
//         if (block->next->used == 0) {
//             block->size = block->size + sizeof(block_header_t) + block->next->size;
//             block->next = block->next->next;
//         }
//     }

//     // we should check if the previous block is also free, to consolitate with that...
//     block_header_t *prev = mem_bottom;
//     while (prev->next != block && prev->next != NULL) {
//         prev = prev->next;
//     }
//     // be careful, it has to be continuous memory area
//     // therefore, adding the header size + block size should give us the address of our block
//     if (prev != NULL
//         && prev->used == 0
//         && ((uint32_t)prev) + sizeof(block_header_t) + prev->size == (uint32_t)block) {
//         prev->size = prev->size + sizeof(block_header_t) + block->size;
//         prev->next = block->next;
//     }
// }

// /*
// void *calloc(int elements, int element_size) {
// return malloc(elements * element_size);
// }

// void *realloc(void *address, size_t new_size) {
//     // expand or shrink the block
//     // in place if possible, otherwise copy to new block
// }

// void free(void *address) {

// }
// */






#define PGSIZE             4096
#define ROUNDDOWN(address) (((adress)) & ~(PGSIZE-1))
#define ROUNDUP(size)      (((size)+PGSIZE-1) & ~(PGSIZE-1))
struct free_page_info {
    struct free_page_info *next_free_page;
};

struct free_page_info *first_free_page;

void init_kernel_pages(size_t start, size_t end) {
    first_free_page = NULL;
    start = ROUNDUP(start);
    end = ROUNDUP(end);
    void *address;
    struct free_page_info *page_info;
    for (address = start; address < end; address += PGSIZE) {
        page_info = (struct free_page_info *)address;
        page_info->next_free_page = first_free_page;
        first_free_page = page_info;
    }
}

size_t kernel_page_size() {
    return PGSIZE;
}

void *allocate_kernel_page() {
    if (first_free_page == NULL)
        panic("No more free pakes to allocate for kernel");
    void *ptr = (void *)first_free_page;
    first_free_page = first_free_page->next_free_page;
    return ptr;
}

void free_kernel_page(void *address) {
    struct free_page_info *page_info = (struct free_page_info *)address;
    page_info->next_free_page = first_free_page;
    first_free_page = page_info;
}
