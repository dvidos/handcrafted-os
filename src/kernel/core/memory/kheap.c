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
    uint16_t magic1: 12;        // AA uA   (u = used)
    uint16_t used: 4;           
    uint32_t size;              // ls nn nn ms (little endian)
    struct memory_block *next;  // ls nn nn ms (split into the two octets)
    struct memory_block *prev;  // ls nn nn ms 
    #ifdef DEBUG_HEAP_OPS
        char *size_explanation;        // 
        char *file;             // xx xx xx xx (split into the two octets)
        uint16_t ff_indicator;  // FF FF 
        uint16_t line;          // ls ms (e.g. 27 00) is actually 0x0027 = 39
    #endif
    uint16_t magic2;            // AA 0A
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

static bool __check_block(memory_block_t *block);


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

// allocate a chunk of memory from kernel heap
void *__kmalloc(size_t size, char *explanation, char *file, uint16_t line) {
    // size = size < 256 ? 256 : size;
    
    // find the first free block that is equal or larger than size
    memory_block_t *curr = kernel_heap.list_head;
    while (curr != NULL) {
        // look for a free block that is either the exact size, or big enough to split.
        if ((curr->used == false) &&
            (curr->size == size || curr->size > size + sizeof(memory_block_t)))
            break;
        curr = curr->next;
    }
    if (curr == NULL) {
        klog_warn("kmalloc(%u) -> Could not find a free block, returning null", size);
        panic("malloc failed. what now?");
        return NULL;
    }

    if (curr->size == size) {
        // no need to split, just reuse this as is.
        // happens often, as the same structure types may be requested in a loop
    } else if (curr->size > size + sizeof(memory_block_t)) {
        // big enough to split into two
        // keep the memory we need, create a new memory block
        // sequence of operations (pointers etc) is important
        memory_block_t *new_free = (memory_block_t *)((char *)curr + sizeof(memory_block_t) + size);
        memory_block_t *next = curr->next;
        new_free->size = curr->size - sizeof(memory_block_t) - size;
        new_free->used = 0;
        new_free->magic1 = KMEM_MAGIC;
        new_free->magic2 = KMEM_MAGIC;
        #ifdef DEBUG_HEAP_OPS
            new_free->size_explanation = NULL;
            new_free->file = NULL;
            new_free->ff_indicator = 0;
            new_free->line = 0;
        #endif
        new_free->prev = curr;
        new_free->next = next;
        kernel_heap.available_memory -= sizeof(memory_block_t);

        curr->size = size;
        curr->next = new_free;
        if (next != NULL)
            next->prev = new_free;
    }

    // in any case do the following:
    #ifdef DEBUG_HEAP_OPS
        curr->size_explanation = explanation;
        curr->file = file;
        curr->ff_indicator = 0xFFFF;
        curr->line = line;
    #endif
    curr->used = 1;
    kernel_heap.available_memory -= curr->size; // we should take memory_block size into account 
    char *ptr = (char *)curr + sizeof(memory_block_t);
    memset(ptr, 0, curr->size); // contrary to traditional unix, we clear our memory

    klog_trace("kmalloc(%u = %s) -> 0x%p, at %s:%d", size, explanation, ptr, file, line);
    return ptr;
}


// checks magic numbers and logs possible overflow/underflow
void __kcheck(void *ptr, char *name, char *file, int line) {
    klog_trace("kcheck(0x%p, \"%s\") at %s:%d", ptr, name, file, line);
    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    bool healthy = false;

    if (ptr < kernel_heap.start_address || ptr > kernel_heap.end_address) {
        klog_critical("- ptr 0x%p (%s): pointer is not managed by kernel heap (heap is 0x%x..0x%x)", 
            ptr, name, kernel_heap.start_address, kernel_heap.end_address);
        healthy = false;
    } else {
        // these checks only make sense if we deal with our allocated pointers
        healthy = __check_block(block);
        if (!healthy) {
            void *addr = ptr - 0x60; // ample room to detet underflow/overflow
            int size = 0x60 + sizeof(memory_block_t) + 0x60;
            klog_hex16_debug(addr, size, (uint32_t)addr);
            kernel_heap_dump();
        }
    }

    if (!healthy)
        panic("Kernel heap corruption detected!");
}

void kfree(void *ptr) {
    klog_trace("kfree(0x%p)", ptr);

    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    memory_block_t *next = block->next;
    memory_block_t *prev = block->prev;
    
    block->used = false;
    kernel_heap.available_memory += block->size;
    #ifdef DEBUG_HEAP_OPS
        block->file = NULL;
        block->line = 0;
        block->ff_indicator = 0;
        block->size_explanation = NULL;
    #endif

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

uint32_t kernel_heap_block_size() {
    return sizeof(memory_block_t);
}

void kernel_heap_dump() {
    memory_block_t *block = kernel_heap.list_head;
    klog_debug("  Pointer      Size Type Magic   Prev     Next     Alloc file:line - size explanation");
    //         "  0x000000 00000000 Used XXX XXX 0xXXXXXX 0xXXXXXX
    uint32_t free_mem = 0;
    uint32_t used_mem = 0;
    uint32_t free_blocks = 0;
    uint32_t used_blocks = 0;
    int n = 0;

    while (block != NULL) {
        if (block->used) {
            used_mem += block->size;
            used_blocks++;
        } else {
            free_mem += block->size;
            free_blocks++;
        }
        #if DEBUG_HEAP_OPS
            klog_debug("  0x%x %8u %s %x %x 0x%x 0x%x %s:%d - %s",
                (uint32_t)block + sizeof(memory_block_t), block->size,
                block->used ? "Used" : "Free",
                block->magic1, block->magic2,
                block->prev, block->next,
                block->file, block->line,
                block->size_explanation
            );
        #else
            klog_debug("  0x%x %8u %s %x %x 0x%x 0x%x",
                (uint32_t)block + sizeof(memory_block_t), block->size,
                block->used ? "Used" : "Free",
                block->magic1, block->magic2,
                block->prev, block->next
            );
        #endif
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


static bool __check_block(memory_block_t *block) {
    uint32_t max_size = (kernel_heap.end_address - kernel_heap.start_address);
    bool healthy = true;
    void *ptr = ((void *)block) + sizeof(memory_block_t);

    if (block->magic1 != KMEM_MAGIC) {
        klog_error("block for 0x%x has bad magic 1 (%x)", ptr, block->magic1);
        healthy = false;
    }
    if (block->magic2 != KMEM_MAGIC) {
        klog_error("block for 0x%x bad magic 2 (%x)", ptr, block->magic2);
        healthy = false;
    }
    if (block->size > max_size) {
        klog_error("block for 0x%x bad size (%d, max is %d)", ptr, block->size, max_size);
        healthy = false;
    }
    if (block->prev != NULL && (
        (void *)block->prev < kernel_heap.start_address ||
        (void *)block->prev > kernel_heap.end_address)) {
        klog_error("block for 0x%x prev ptr outside heap boundaries (0x%x)", ptr, block->prev, max_size);
        healthy = false;
    }
    if (block->next != NULL && (
        (void *)block->next < kernel_heap.start_address ||
        (void *)block->next > kernel_heap.end_address)) {
        klog_error("block for 0x%x next ptr outside heap boundaries (0x%x)", ptr, block->next, max_size);
        healthy = false;
    }

    #ifdef DEBUG_HEAP_OPS
        bool is_last_block = block->size == 0 && block->next == NULL;
        if (block->used && !is_last_block) {
            if (block->ff_indicator != 0xFFFF) {
                klog_error("block for 0x%x bad ff indicator (0x%x)", ptr, block->ff_indicator);
                healthy = false;
            }
            if (block->file == NULL || strlen(block->file) == 0 || strlen(block->file) > 120) {
                klog_error("block for 0x%x invalid file indicator (%s)", ptr, block->file);
                healthy = false;
            }
            if (block->line <= 0 || block->line > 10000) {
                klog_error("block for 0x%x invalid line indicator (%d)", ptr, block->line);
                healthy = false;
            }
            if (block->size_explanation == NULL || strlen(block->size_explanation) == 0 || strlen(block->size_explanation) > 120) {
                klog_error("block for 0x%x invalid size explanation (%s)", ptr, block->size_explanation);
                healthy = false;
            }
        }
    #endif

    return healthy;
}

// walk the heap blocks and assert sanity values
void __kernel_heap_verify(char *file, int line) {
    bool healthy = true;
    memory_block_t *block = kernel_heap.list_head;
    while (block != NULL) {
        // make sure the block is healthy
        if (!__check_block(block))
            healthy = false;
        block = block->next;
    }

    if (healthy) {
        klog_debug("kernel heap healthy at %s:%d", file, line);
    } else {
        klog_critical("kernel heap issues, detected at %s:%d", file, line);
        kernel_heap_dump();
        panic("Bad heap state");
    }
}

