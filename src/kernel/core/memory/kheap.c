#include <drivers/screen.h>
#include <klib/string.h>
#include <klog.h>
#include <memory/physmem.h>
#include <memory/kheap.h>

#define KMEM_MAGIC       0xAAA // something that fits in 12 bits


// doubly linked list allows fast consolidation with prev / next blocks
// magic number allows detection of underflow or overflow
// the memory area is supposed to be right after the block_header.
// size refers to the memory area, does not include the memory_block structure
// sandwich with magic numbers to detect even partial corruption.
struct memory_block {
    uint16_t magic1: 12;
    uint16_t used: 4;
    uint32_t size;
    struct memory_block *next;
    struct memory_block *prev;
    #ifdef DEBUG_HEAP_OPS
        char *file;
        uint16_t line;
    #endif
    uint16_t magic2;
} __attribute__((packed));
typedef struct memory_block memory_block_t;

struct memory_heap {
    void *start_address;
    void *end_address;
    uint32_t available_memory; // remaining actual memory, leaves out allocated and blocks
    memory_block_t *list_head;
    memory_block_t *list_tail;
};
typedef struct memory_heap memory_heap_t;

memory_heap_t kernel_heap;

void init_kernel_heap(size_t heap_size, void *minimum_address) {
    void *heap = allocate_consecutive_physical_pages(heap_size, minimum_address);
    if (heap == NULL)
        panic("Failed allocating physical memory for kernel heap");
    kernel_heap.start_address = heap;
    kernel_heap.end_address = heap + heap_size;
    kernel_heap.available_memory = heap_size - 2 * sizeof(memory_block_t);

    // putting a block at the end of the area, to detect possible overflow
    memory_block_t *head = (memory_block_t *)(kernel_heap.start_address);
    memory_block_t *tail = (memory_block_t *)(kernel_heap.end_address - sizeof(memory_block_t));

    head->used = 0;
    head->size = heap_size - 2 * sizeof(memory_block_t);
    head->magic1 = KMEM_MAGIC;
    head->magic2 = KMEM_MAGIC;
    head->next = tail;
    head->prev = NULL;
    
    tail->used = 1; // tail marked used to avoid consolidation
    tail->size = 0;
    tail->magic1 = KMEM_MAGIC;
    tail->magic2 = KMEM_MAGIC;
    tail->prev = head;
    tail->next = NULL;

    kernel_heap.list_head = head;
    kernel_heap.list_tail = tail;
}

// allocate a chunk of memory. min size restrictions apply
void *__kmalloc(size_t size, char *file, uint16_t line) {
    // size = size < 256 ? 256 : size;
    
    // find the first free block that is equal or larger than size
    memory_block_t *curr = kernel_heap.list_head;
    while (curr != NULL && (curr->used || curr->size < size))
        curr = curr->next;
    if (curr == NULL) {
        klog_warn("kmalloc(%u) -> Could not find a free block, returning null", size);
        panic("malloc failed. what now?");
        return NULL;
    }

    // keep the memory we need, create a new memory block
    // sequence of operations (pointers etc) is important
    memory_block_t *new_free = (memory_block_t *)((char *)curr + sizeof(memory_block_t) + size);
    memory_block_t *next = curr->next;
    new_free->size = curr->size - sizeof(memory_block_t) - size;
    new_free->used = 0;
    new_free->magic1 = KMEM_MAGIC;
    new_free->magic2 = KMEM_MAGIC;
    #ifdef DEBUG_HEAP_OPS
        new_free->file = file;
        new_free->line = line;
    #endif
    new_free->prev = curr;
    new_free->next = next;
    kernel_heap.available_memory -= sizeof(memory_block_t);

    curr->size = size;
    curr->next = new_free;
    if (next != NULL)
        next->prev = new_free;
    
    curr->used = 1;
    kernel_heap.available_memory -= curr->size;
    char *ptr = (char *)curr + sizeof(memory_block_t);
    memset(ptr, 0, curr->size); // contrary to unix, we clear our memory

    klog_trace("kmalloc(%u) -> 0x%p, at %s:%d", size, ptr, file, line);
    return ptr;
}

// checks magic numbers and logs possible overflow/underflow
void __kcheck(void *ptr, char *name, char *file, int line) {
    klog_trace("kcheck(0x%p, \"%s\") at %s:%d", ptr, name, file, line);

    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    memory_block_t *next = block->next;
    memory_block_t *prev = block->prev;
    bool issue = false;

    if (ptr >= kernel_heap.start_address && ptr <= kernel_heap.end_address) {
        // these checks only make sense if we deal with our allocated pointers

        if (block->magic1 != KMEM_MAGIC || block->magic2 != KMEM_MAGIC) {
            klog_critical("- ptr 0x%p (%s): buffer underflow detected, some content follows", ptr, name);
            klog_hex16_debug(((char*)block) - 0x70, sizeof(memory_block_t) * 10, ((uint32_t)block) - 0x70);
        }

        if (next != NULL && ((void *)next < kernel_heap.start_address || (void *)next > kernel_heap.end_address)) {
            klog_critical("- ptr 0x%p (%s): next block ptr outside of heap area (ptr=0x%x, heap 0x%x..0x%x)", 
                ptr, name, next, kernel_heap.start_address, kernel_heap.end_address);
            issue = true;
        }
        if (prev != NULL && ((void *)prev < kernel_heap.start_address || (void *)prev > kernel_heap.end_address)) {
            klog_critical("- ptr 0x%p (%s): prev block ptr outside of heap area (ptr=0x%x, heap 0x%x..0x%x)", 
                ptr, name, prev, kernel_heap.start_address, kernel_heap.end_address);
            issue = true;
        }
        if (next != NULL && (next->magic1 != KMEM_MAGIC || next->magic2 != KMEM_MAGIC)) {
            klog_critical("- ptr 0x%p (%s): buffer overflow detected, some content follows", ptr, name);
            klog_hex16_debug(((char*)block) - 0x20, sizeof(memory_block_t) * 10, ((uint32_t)block) - 0x20);
            issue = true;
        }
        if (!block->used) {
            klog_critical("- ptr 0x%p (%s): block seems not allocated", ptr, name);
            issue = true;
        }
    } else {
        klog_critical("- ptr 0x%p (%s): pointer is not managed by kernel heap (heap is 0x%x..0x%x)", 
            ptr, name, kernel_heap.start_address, kernel_heap.end_address);
        issue = true;
    }

    if (issue)
        panic("Kernel heap corruption detected!");
}

void kfree(void *ptr) {
    klog_trace("kfree(0x%p)", ptr);

    kcheck(ptr, "kfree(ptr)");

    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    memory_block_t *next = block->next;
    memory_block_t *prev = block->prev;
    
    block->used = false;
    kernel_heap.available_memory += block->size;

    // clean up memory to cause errors if app still refers to it.
    memset((char *)block + sizeof(memory_block_t), 0, block->size);

    // Initial state
    // [ mbt ] ----next---> [ mbt ] ----next---> [ mbt ] ----next---> [ mbt ]
    // [ mbt ] <---prev---- [ mbt ] <---prev---- [ mbt ] <---prev---- [ mbt ]
    //    ^                    ^                    ^
    //   prev                block                 next

    // Step 1 - consolidate with next block
    // [ mbt ] ----next---> [ mbt ] --------------next--------------> [ mbt ]
    // [ mbt ] <---prev---- [ mbt ] <-------------prev--------------- [ mbt ]
    //    ^                    ^                    ^
    //   prev                block                 next

    // Step 2 - consolidate with prevblock
    // [ mbt ] -------------------------next------------------------> [ mbt ]
    // [ mbt ] <------------------------prev------------------------- [ mbt ]
    //    ^                    ^                    ^
    //   prev                block                 next

    // see if consolidatable with next (if so, remove next block, keep this)
    if (next != NULL && !next->used) {
        block->next = next->next;
        if (next->next != NULL)
            next->next->prev = block;
        block->size += sizeof(memory_block_t) + next->size;
        kernel_heap.available_memory += sizeof(memory_block_t);
    }
    if (prev != NULL && !prev->used) {
        prev->next = block->next;
        if (block->next != NULL)
            block->next->prev = prev;
        prev->size += sizeof(memory_block_t) + block->size;
        kernel_heap.available_memory += sizeof(memory_block_t);
    }
}

// returns the amount of memory the heap is managing
uint32_t kernel_heap_total_size() {
    return (kernel_heap.end_address - kernel_heap.start_address);
}

// returns the remaining memory size that can be allocated
uint32_t kernel_heap_free_size() {
    return kernel_heap.available_memory;
}

void kernel_heap_dump() {
    memory_block_t *block = kernel_heap.list_head;
    klog_debug("  Address         Size  Type  Magic  Prev        Next");
    //      0x00000000  00000000  Used  XXXX   0xXXXXXXXX  0xXXXXXXXX
    uint32_t free_mem = 0;
    uint32_t used_mem = 0;
    uint32_t free_blocks = 0;
    uint32_t used_blocks = 0;

    while (block != NULL) {
        if (block->used) {
            used_mem += block->size;
            used_blocks++;
        } else {
            free_mem += block->size;
            free_blocks++;
        }
        klog_debug("  0x%08x  %8u  %s  %x   %x   0x%08x  0x%08x",
            (uint32_t)block,
            block->size,
            block->used ? "Used" : "Free",
            block->magic1,
            block->magic2,
            block->prev,
            block->next
        );
        block = block->next;
    }
    free_mem /= 1024;
    used_mem /= 1024;
    int utilization = (used_mem * 100) / (free_mem + used_mem);

    int percent_free = (kernel_heap.available_memory * 100) / (kernel_heap.end_address - kernel_heap.start_address);
    klog_debug("Free memory %u KB (%u%%), out of %u KB total",
        kernel_heap.available_memory / 1024,
        percent_free,
        (kernel_heap.end_address - kernel_heap.start_address) / 1024
    );
    klog_debug("Total free memory  %u KB (%u blocks)", (uint32_t)free_mem, free_blocks);
    klog_debug("Total used memory  %u KB (%u blocks) - %d%% utilization", (uint32_t)used_mem, used_blocks, utilization);
}

