#include "internal.h"


static int range_block_last_block_no(mounted_data *mt, uint32_t range_block_no, uint32_t *last_block_no) {
    int err;

    err = bcache_read(mt->cache, range_block_no, 0, 
        mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    err = range_array_last_block_no((block_range *)mt->generic_block_buffer, ranges_per_block, last_block_no);
    if (err != OK) return err;

    return OK;
}

static int range_block_initialize(mounted_data *mt, uint32_t range_block_no, uint32_t *new_block_no) { // assumes range_block is already allocated
    int err;
    int overflown;

    err = bcache_wipe(mt->cache, range_block_no);
    if (err != OK) return err;

    err = bcache_read(mt->cache, range_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    err = range_array_expand((block_range *)mt->generic_block_buffer, ranges_per_block, mt->bitmap, new_block_no, &overflown);
    if (err != OK) return err;

    if (overflown) // we should not overflow for a new block...
        return ERR_CORRUPTION_DETECTED;
    
    err = bcache_write(mt->cache, range_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    return OK;
}

static int range_block_expand(mounted_data *mt, uint32_t range_block_no, uint32_t *new_block_no, int *overflown) {
    int err;

    err = bcache_read(mt->cache, range_block_no, 0, 
        mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    err = range_array_expand((block_range *)mt->generic_block_buffer, ranges_per_block, mt->bitmap, new_block_no, overflown);
    if (err != OK) return err;

    if (*overflown)
        return OK; // no point in saving, caller needs to deal with this
    
    err = bcache_write(mt->cache, range_block_no, 0, 
        mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    return OK;
}

static int range_block_resolve_indirect_index(mounted_data *mt, uint32_t indirect_block_no, uint32_t *block_index, uint32_t *block_no) {
    int err;
    
    err = bcache_read(mt->cache, indirect_block_no, 0, 
        mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
        if (err != OK) return err;
        
        int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
        err = range_array_resolve_index((block_range *)mt->generic_block_buffer, ranges_per_block, block_index, block_no);
        if (err != OK) return err;
        
    return OK;
}

static int range_block_resolve_dbl_indirect_index(mounted_data *mt, uint32_t dbl_indirect_block_no, uint32_t *block_index, uint32_t *block_no) {
    int err;
    block_range range;
    
    // since the indirect_block_release will use the generic_block_buffer, let's avoid it & use the cache's buffer
    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    for (int i = 0; i <ranges_per_block; i++) {
        err = bcache_read(mt->cache, dbl_indirect_block_no, i * sizeof(block_range), &range, sizeof(block_range));
        if (err != OK) return err;
        if (range.first_block_no == 0)
        break;
        
        for (int b = 0; b < range.blocks_count; b++) {
            err = range_block_resolve_indirect_index(mt, range.first_block_no + b, block_index, block_no);
            if (err != OK) return err;
        }
    }
    
    return OK;
}

static int range_block_release_indirect_block(mounted_data *mt, uint32_t indirect_block_no) {
    int err;

    err = bcache_read(mt->cache, indirect_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    range_array_release_blocks((block_range *)mt->generic_block_buffer, ranges_per_block, mt->bitmap);
    return OK;
}

static int range_block_release_dbl_indirect_block(mounted_data *mt, uint32_t dbl_indirect_block_no) {
    int err;
    block_range range;

    // since the indirect_block_release will use the generic_block_buffer, let's avoid it & use the cache's buffer
    int ranges_per_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    for (int i = 0; i <ranges_per_block; i++) {
        err = bcache_read(mt->cache, dbl_indirect_block_no, i * sizeof(block_range), &range, sizeof(block_range));
        if (err != OK) return err;
        if (range.first_block_no == 0)
            break;
        
        for (int b = 0; b < range.blocks_count; b++) {
            err = range_block_release_indirect_block(mt, range.first_block_no + b);
            if (err != OK) return err;
        }
    }

    return OK;
}


