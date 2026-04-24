#ifndef _MEMORY_H
#define _MEMORY_H

#include "types.h"


void init_heap(word start, word end);
word heap_free_space();
void *malloc(int size);

struct mem_map_entry_24 {
    uint32 address_low;
    uint32 address_high;
    uint32 size_low;
    uint32 size_high;
    uint32 type;
    uint32 padding;
};


void print_mem_map_entry(struct mem_map_entry_24 *entry);
void detect_memory();



#endif
