#pragma once

#include <stdlib.h>
#include "../dependencies/mem_allocator.h"
#include "../dependencies/sector_device.h"

// --- caching layer -------------

// keeps one or more blocks in memory, reads and writes in memory, flushes as needed
typedef struct cache_layer cache_layer;
struct cache_layer {
    int (*read)(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length);
    int (*write)(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length);
    int (*wipe)(cache_layer *cl, uint32_t block_no);  // wipes a block with zeros, in prep for writing
    int (*flush)(cache_layer *cl);
    int (*release_memory)(cache_layer *cl);
    void *data;
};

cache_layer *new_cache_layer(mem_allocator *memory, sector_device *device, int block_size_bytes, int blocks_to_cache);

// -------------------------------------------


