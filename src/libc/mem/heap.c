
#define MEM_MAGIC             0x6A3E // something that fits in 14 bits


// doubly linked list allows fast consolidation with prev / next blocks
// magic number allows detection of underflow or overflow
// the memory area is supposed to be right after the block_header.
// size refers to the memory area, does not include the memory_block structure
struct memory_block {
    uint32_t used: 1;
    uint32_t magic: 15;
    uint32_t padding: 16; // to make the memory_block size 16 bytes
    uint32_t size;
    struct memory_block *next;
    struct memory_block *prev;
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

memory_heap_t heap;

void set_heap(void *start_address, void *end_address) {
    uint32_t heap_size = (uint32_t)end_address - (uint32_t)start_address;
    heap.start_address = start_address;
    heap.end_address = end_address;
    heap.available_memory = heap_size - 2 * sizeof(memory_block_t);

    // putting a block at the end of the area, to detect possible overflow
    memory_block_t *head = (memory_block_t *)(heap.start_address);
    memory_block_t *tail = (memory_block_t *)(heap.end_address - sizeof(memory_block_t));

    head->used = 0;
    head->size = heap_size - 2 * sizeof(memory_block_t);
    head->magic = MEM_MAGIC;
    head->next = tail;
    head->prev = NULL;
    
    tail->used = 1; // tail marked used to avoid consolidation
    tail->size = 0;
    tail->magic = MEM_MAGIC;
    tail->prev = head;
    tail->next = NULL;

    heap.list_head = head;
    heap.list_tail = tail;
}

// allocate a chunk of memory. min size restrictions apply
void *malloc(size_t size) {
    size = size < 256 ? 256 : size;
    
    // find the first free block that is equal or larger than size
    memory_block_t *curr = heap.list_head;
    while (curr != NULL && (curr->used || curr->size < size))
        curr = curr->next;
    if (curr == NULL)
        return NULL;

    // keep the memory we need, create a new memory block
    // sequence of operations (pointers etc) is important
    memory_block_t *new_free = (memory_block_t *)((char *)curr + sizeof(memory_block_t) + size);
    memory_block_t *next = curr->next;
    new_free->size = curr->size - sizeof(memory_block_t) - size;
    new_free->used = 0;
    new_free->magic = MEM_MAGIC;
    new_free->prev = curr;
    new_free->next = next;
    heap.available_memory -= sizeof(memory_block_t);

    curr->size = size;
    curr->next = new_free;
    if (next != NULL)
        next->prev = new_free;
    
    curr->used = 1;
    heap.available_memory -= curr->size;
    char *p = (char *)curr + sizeof(memory_block_t);
    memset(p, 0, curr->size);
    return p;
}

void free(void *ptr) {
    memory_block_t *block = (memory_block_t *)(ptr - sizeof(memory_block_t));
    memory_block_t *next = block->next;
    memory_block_t *prev = block->prev;

    if (block->magic != MEM_MAGIC)
        fprintf(stderr, "Buffer underflow detected");
    if (next != NULL && next->magic != MEM_MAGIC)
        fprintf(stderr, "Buffer overflow detected");
    if (!block->used)
        return; // already freed
    
    block->used = false;
    heap.available_memory += block->size;

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

// returns the remaining memory size that can be allocated
uint32_t heap_free_size() {
    return heap.available_memory;
}

void heap_dump() {
    memory_block_t *block = heap.list_head;
    fprintf(stderr, "  Address         Size  Type  Magic  Prev        Next\n");
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
        fprintf(stderr, "  0x%08x  %8u  %s  %x   0x%08x  0x%08x\n",
            (uint32_t)block,
            block->size,
            block->used ? "Used" : "Free",
            block->magic,
            block->prev,
            block->next
        );
        block = block->next;
    }
    free_mem /= 1024;
    used_mem /= 1024;
    int utilization = (used_mem * 100) / (free_mem + used_mem);

    int percent_free = (heap.available_memory * 100) / (heap.end_address - heap.start_address);
    fprintf(stderr, "Free memory %u KB (%u%%), out of %u KB total\n",
        heap.available_memory / 1024,
        percent_free,
        (heap.end_address - heap.start_address) / 1024
    );
    fprintf(stderr, "Total free memory  %u KB (%u blocks)\n", (uint32_t)free_mem, free_blocks);
    fprintf(stderr, "Total used memory  %u KB (%u blocks) - %d%% utilization\n", (uint32_t)used_mem, used_blocks, utilization);
}
