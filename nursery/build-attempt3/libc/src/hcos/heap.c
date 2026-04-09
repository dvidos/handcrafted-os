#include "../libc_internal.h"



#define MEM_MAGIC       0xAAA // something that fits in 12 bits


// doubly linked list allows fast consolidation with prev / next blocks
// magic number allows detection of underflow or overflow
// the memory area is supposed to be right after the block_header.
// size refers to the memory area, does not include the memory_block structure
// sandwich with magic numbers to detect even partial corruption.
struct memory_block {
    uint16_t magic1: 12;        // AAuA   (u = used)
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


// in theory, in the data section of each executable
// we are supposed to 
memory_heap_t heap = { 0, 0, 0, 0, 0 };

static bool __check_block(memory_block_t *block);



// request to expand or shrink the heap by an amount
// returns pointer to the break before the change.
void *sbrk(int size_diff) {
    return (void *)syscall(SYS_SBRK, size_diff, 0, 0, 0, 0);
}




// called before main(), no need to be called by user
void __init_heap() {

    // in theory heap is initialized to zero, so the old break value will be the heap start
    void *heap_start = sbrk(4096);
    void *heap_end = sbrk(0);
    size_t heap_size = heap_end - heap_start;

    heap.start_address = heap_start;
    heap.end_address = heap_start + heap_size;
    heap.available_memory = heap_size - 2 * sizeof(memory_block_t);
    // putting a block at the end of the area, to detect possible overflow
    memory_block_t *head = (memory_block_t *)(heap.start_address);
    memory_block_t *tail = (memory_block_t *)(heap.end_address - sizeof(memory_block_t));

    head->used = 0;
    head->size = heap_size - 2 * sizeof(memory_block_t);
    head->magic1 = MEM_MAGIC;
    head->magic2 = MEM_MAGIC;
    head->next = tail;
    head->prev = NULL;
    
    tail->used = 1; // tail marked used to avoid consolidation
    tail->size = 0;
    tail->magic1 = MEM_MAGIC;
    tail->magic2 = MEM_MAGIC;
    tail->prev = head;
    tail->next = NULL;

    heap.list_head = head;
    heap.list_tail = tail;

    // syslog_debug("Heap initialized, %d bytes at 0x%x", heap.end_address - heap.start_address, heap.start_address);
}

// allocate a chunk of memory from heap
void *__malloc(size_t size, char *explanation, char *file, uint16_t line) {
    // size = size < 256 ? 256 : size;
    
    // find the first free block that is equal or larger than size
    memory_block_t *curr = heap.list_head;
    while (curr != NULL) {
        // look for a free block that is either the exact size, or big enough to split.
        if ((curr->used == false) &&
            (curr->size == size || curr->size > size + sizeof(memory_block_t)))
            break;
        curr = curr->next;
    }
    if (curr == NULL) {
        syslog_warn("malloc(%u) --> Exiting, as could not find a free block, should sbrk() and extend last block", size);
        exit(-8);
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
        new_free->magic1 = MEM_MAGIC;
        new_free->magic2 = MEM_MAGIC;
        #ifdef DEBUG_HEAP_OPS
            new_free->size_explanation = NULL;
            new_free->file = NULL;
            new_free->ff_indicator = 0;
            new_free->line = 0;
        #endif
        new_free->prev = curr;
        new_free->next = next;
        heap.available_memory -= sizeof(memory_block_t);

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
    heap.available_memory -= curr->size; // we should take memory_block size into account 
    char *ptr = (char *)curr + sizeof(memory_block_t);
    memset(ptr, 0, curr->size); // contrary to traditional unix, we clear our memory

    // syslog_trace("malloc(%u = %s) -> 0x%p, at %s:%d", size, explanation, ptr, file, line);
    return ptr;
}


// Reallocate memory block
void *realloc(void *ptr, size_t size) {
    // 1. Handle ptr == NULL: Behaves like malloc(size).
    if (ptr == NULL) {
        return malloc(size); // Use the macro for consistency
    }

    // 2. Handle size == 0: Behaves like free(ptr).
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // Get the memory_block_t header
    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));

    // Basic validation (optional, but good for debugging)
    if (block->used == 0 || block->magic1 != MEM_MAGIC || block->magic2 != MEM_MAGIC) {
        syslog_error("realloc(%p, %u): Invalid or free block.", ptr, size);
        errno = EFAULT; // Or some other appropriate error
        return NULL;
    }

    // Get current user data size
    size_t current_size = block->size;

    // Case A: New size is smaller or equal
    if (size <= current_size) {
        // Only split if the new size is at most half as big as the original,
        // and there's enough space for a new block header + a minimal free block size.
        // Assuming minimal useful free block size is > 0 bytes.
        if (size <= current_size / 2 && current_size > size + sizeof(memory_block_t)) {
            // Shrink and split
            memory_block_t *new_free = (memory_block_t *)((char *)block + sizeof(memory_block_t) + size);
            memory_block_t *next = block->next;

            new_free->size = current_size - sizeof(memory_block_t) - size;
            new_free->used = 0;
            new_free->magic1 = MEM_MAGIC;
            new_free->magic2 = MEM_MAGIC;
            #ifdef DEBUG_HEAP_OPS
                new_free->size_explanation = NULL;
                new_free->file = NULL;
                new_free->ff_indicator = 0;
                new_free->line = 0;
            #endif
            new_free->prev = block;
            new_free->next = next;

            block->size = size;
            block->next = new_free;
            if (next != NULL) {
                next->prev = new_free;
            }
            heap.available_memory -= sizeof(memory_block_t); // A new block header was created

            // Clean up the newly freed part of memory
            memset((char*)new_free + sizeof(memory_block_t), 0, new_free->size);
            syslog_trace("realloc(%p, %u): shrunk block and split new free block %p", ptr, size, (char*)new_free + sizeof(memory_block_t));
        }
        // If not enough to split (either size is not small enough, or remaining space is too small),
        // just keep the block as is. No data movement needed.
        return ptr;
    }

    // Case B: New size is larger
    // Try to extend the current block by merging with next free block
    memory_block_t *next_block = block->next;
    if (next_block != NULL && !next_block->used &&
        (current_size + sizeof(memory_block_t) + next_block->size >= size)) {
        syslog_trace("realloc(%p, %u): attempting to merge with next free block %p", ptr, size, (char*)next_block + sizeof(memory_block_t));

        // Remove next_block from the list
        block->next = next_block->next;
        if (next_block->next != NULL) {
            next_block->next->prev = block;
        }

        heap.available_memory += sizeof(memory_block_t); // next_block header is being absorbed

        // Update current block's size
        block->size += sizeof(memory_block_t) + next_block->size;

        // Clean up the memory of the absorbed block (already done by memset below if splitting)
        // memset((char*)next_block + sizeof(memory_block_t), 0, next_block->size);

        // Now, the block is larger. If it's *much* larger, we can split it.
        if (block->size > size + sizeof(memory_block_t)) {
             // Split off a new free block from the end
            memory_block_t *new_free = (memory_block_t *)((char *)block + sizeof(memory_block_t) + size);
            memory_block_t *orig_next = block->next; // This is the block that was originally after next_block

            new_free->size = block->size - sizeof(memory_block_t) - size;
            new_free->used = 0;
            new_free->magic1 = MEM_MAGIC;
            new_free->magic2 = MEM_MAGIC;
            #ifdef DEBUG_HEAP_OPS
                new_free->size_explanation = NULL;
                new_free->file = NULL;
                new_free->ff_indicator = 0;
                new_free->line = 0;
            #endif
            new_free->prev = block;
            new_free->next = orig_next;

            block->size = size;
            block->next = new_free;
            if (orig_next != NULL) {
                orig_next->prev = new_free;
            }
            heap.available_memory -= sizeof(memory_block_t); // A new block header was created
            memset((char*)new_free + sizeof(memory_block_t), 0, new_free->size); // Clear the new free memory
            syslog_trace("realloc(%p, %u): merged with next free and then split", ptr, size);
        }
        return ptr; // Return the same pointer
    }

    // If unable to extend, allocate new memory, copy, and free old
    syslog_trace("realloc(%p, %u): allocating new block, copying, freeing old", ptr, size);
    void *new_ptr = malloc(size); // Use the macro for consistency
    if (new_ptr == NULL) {
        // Malloc failed, errno is already set
        return NULL;
    }

    // Copy original data, up to the smaller of current_size or new size
    memcpy(new_ptr, ptr, (current_size < size) ? current_size : size);

    free(ptr); // Free the old block

    return new_ptr;
}

void free(void *ptr) {
    // syslog_trace("free(0x%p)", ptr);

    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    memory_block_t *next = block->next;
    memory_block_t *prev = block->prev;
    
    block->used = false;
    heap.available_memory += block->size;
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
        heap.available_memory += sizeof(memory_block_t);
    }
    if (prev != NULL && !prev->used) {
        prev->next = block->next;
        if (block->next != NULL)
            block->next->prev = prev;
        prev->size += sizeof(memory_block_t) + block->size;
        heap.available_memory += sizeof(memory_block_t);
    }
}

// returns the amount of memory the heap is managing
uint32_t heap_total_size() {
    return (heap.end_address - heap.start_address);
}

// returns the remaining memory size that can be allocated
uint32_t heap_free_size() {
    return heap.available_memory;
}

void heap_dump() {
    memory_block_t *block = heap.list_head;
    syslog_debug("  Pointer      Size Type Magic   Prev     Next     Alloc file:line - size explanation", 0);
    //         "  0x000000 00000000 Used XXX XXX 0xXXXXXX 0xXXXXXX
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
        #if DEBUG_HEAP_OPS
            syslog_debug("  0x%x %8u %s %x %x 0x%x 0x%x %s:%d - %s",
                (uint32_t)block + sizeof(memory_block_t), block->size,
                block->used ? "Used" : "Free",
                block->magic1, block->magic2,
                block->prev, block->next,
                block->file, block->line,
                block->size_explanation
            );
        #else
            syslog_debug("  0x%x %8u %s %x %x 0x%x 0x%x",
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

    int percent_free = (heap.available_memory * 100) / (heap.end_address - heap.start_address);
    syslog_debug("Free memory %u KB (%u%%), out of %u KB total",
        heap.available_memory / 1024,
        percent_free,
        (heap.end_address - heap.start_address) / 1024
    );
    syslog_debug("Total free memory  %u KB (%u blocks)", (uint32_t)free_mem, free_blocks);
    syslog_debug("Total used memory  %u KB (%u blocks) - %d%% utilization", (uint32_t)used_mem, used_blocks, utilization);
}


static bool __check_block(memory_block_t *block) {
    uint32_t max_size = (heap.end_address - heap.start_address);
    bool healthy = true;
    void *ptr = ((void *)block) + sizeof(memory_block_t);

    if (block->magic1 != MEM_MAGIC) {
        syslog_error("block for 0x%x has bad magic 1 (%x)", ptr, block->magic1);
        healthy = false;
    }
    if (block->magic2 != MEM_MAGIC) {
        syslog_error("block for 0x%x bad magic 2 (%x)", ptr, block->magic2);
        healthy = false;
    }
    if (block->size > max_size) {
        syslog_error("block for 0x%x bad size (%d, max is %d)", ptr, block->size, max_size);
        healthy = false;
    }
    if (block->prev != NULL && (
        (void *)block->prev < heap.start_address ||
        (void *)block->prev > heap.end_address)) {
        syslog_error("block for 0x%x prev ptr outside heap boundaries (0x%x)", ptr, block->prev, max_size);
        healthy = false;
    }
    if (block->next != NULL && (
        (void *)block->next < heap.start_address ||
        (void *)block->next > heap.end_address)) {
        syslog_error("block for 0x%x next ptr outside heap boundaries (0x%x)", ptr, block->next, max_size);
        healthy = false;
    }

    #ifdef DEBUG_HEAP_OPS
        bool is_last_block = block->size == 0 && block->next == NULL;
        if (block->used && !is_last_block) {
            if (block->ff_indicator != 0xFFFF) {
                syslog_error("block for 0x%x bad ff indicator (0x%x)", ptr, block->ff_indicator);
                healthy = false;
            }
            if (block->file == NULL || strlen(block->file) == 0 || strlen(block->file) > 120) {
                syslog_error("block for 0x%x invalid file indicator (%s)", ptr, block->file);
                healthy = false;
            }
            if (block->line <= 0 || block->line > 10000) {
                syslog_error("block for 0x%x invalid line indicator (%d)", ptr, block->line);
                healthy = false;
            }
            if (block->size_explanation == NULL || strlen(block->size_explanation) == 0 || strlen(block->size_explanation) > 120) {
                syslog_error("block for 0x%x invalid size explanation (%s)", ptr, block->size_explanation);
                healthy = false;
            }
        }
    #endif

    return healthy;
}


// walk the heap blocks and assert sanity values
void __heap_verify(char *file, int line) {
    bool healthy = true;
    memory_block_t *block = heap.list_head;
    while (block != NULL) {
        // make sure the block is healthy
        if (!__check_block(block))
            healthy = false;
        block = block->next;
    }

    if (healthy) {
        syslog_debug("heap healthy at %s:%d", file, line);
    } else {
        syslog_critical("heap issues, detected at %s:%d", file, line);
        heap_dump();
        exit(-9);
    }
}

