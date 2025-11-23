#include "internal.h"


static int read_block_range_low_level(sector_device *dev, uint32_t sector_size, uint32_t sectors_per_block, uint32_t first_block, uint32_t block_count, void *buffer) {
    // very simple to allow us loading the superblock on mounting
    if (dev == NULL)
        return ERR_NOT_SUPPORTED;

    int sector_number = first_block * sectors_per_block;
    int sector_count = block_count * sectors_per_block;
    void *target = buffer;
    while (sector_count-- > 0) {
        int err = dev->read_sector(dev, sector_number, target);
        if (err != OK) return err;
        sector_number += 1;
        target += sector_size;
    }

    return OK;
}

static int read_block(filesys_data *data, uint32_t block_no, void *buffer) {
    if (data->mounted == NULL || data->mounted->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    
    int sector_no = block_no * sb->sectors_per_block;
    for (int i = 0; i < sb->sectors_per_block; i++) {
        int err = data->device->read_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        buffer += sb->sector_size;
        sector_no += 1;
    }

    return OK;
}

static int write_block(filesys_data *data, uint32_t block_no, void *buffer) {
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int sector_no = block_no * sb->sectors_per_block;
    for (int i = 0; i < sb->sectors_per_block; i++) {
        int err = data->device->write_sector(data->device, sector_no, buffer);
        if (err != OK) return err;

        buffer += sb->sector_size;
        sector_no += 1;
    }

    return OK;
}


// given an inode, it resolves the relative block index in the file, into the block_no on the disk
static int find_block_no_from_file_block_index(mounted_data *mt, const inode *inode, uint32_t block_index_in_file, uint32_t *absolute_block_no) {
    if (block_index_in_file >= inode->allocated_blocks)
        return ERR_OUT_OF_BOUNDS;

    // first, try the inline ranges
    for (int i = 0; i < RANGES_IN_INODE; i++) {
        if (is_range_empty(&inode->ranges[i]))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&inode->ranges[i], &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if there is no extra ranges block, we cannot find it
    if (inode->indirect_ranges_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    
    int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    block_range range;
    for (int i = 0; i < ranges_in_block; i++) {
        int err = cached_read(mt->cache, inode->indirect_ranges_block_no, sizeof(block_range) * i, (void *)&range, sizeof(block_range));
        if (err != OK) return err;

        if (is_range_empty(&range))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&range, &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if here, we have exhausted all ranges, relative_block is outside of them all
    // it means we need more ranges, or two-step ranges for bigger files. but... for another day.
    return ERR_OUT_OF_BOUNDS;
}

// try to extend, allocate new, or fail to fallback
static int add_block_to_array_of_ranges(mounted_data *mt, block_range *ranges_array, int ranges_count, int fallback_available, int *use_fallback, uint32_t *new_block_no) {
    int last_used_idx;
    int first_free_idx;
    int err;

    // by default, we won't need to use the fallback.
    *use_fallback = 0;

    find_last_used_and_first_free_range(ranges_array, ranges_count, &last_used_idx, &first_free_idx);
    if (first_free_idx >= 0) {

        // if there is one already use, try to extend it.
        if (last_used_idx >= 0) {
            err = extend_range_by_allocating_block(mt, &ranges_array[last_used_idx], new_block_no);
            // if successful, we are good
            if (err == OK) return OK;
        }

        // there is nothing to extend, or we failed to extend. allocate the free one
        err = initialize_range_by_allocating_block(mt, &ranges_array[first_free_idx], new_block_no);
        if (err != OK) return err;

        return OK;
    }

    // so, no free ranges are available, we may have an indirect block though
    // only if there is one last used, and there is no indirect, there is a reason to try to extend
    if (!fallback_available && last_used_idx >= 0) {
        err = extend_range_by_allocating_block(mt, &ranges_array[last_used_idx], new_block_no);
        // if successful, we are good
        if (err == OK) return OK;
    }

    // there is no fallback, it's a clear failure
    if (!fallback_available) {
        use_fallback = 0;
        return ERR_RESOURCES_EXHAUSTED;
    }
    
    // in all other cases (including failing to extend), we need the indirect block
    *use_fallback = 1;
    return OK;
}

// allocate and add a data block at the end of a file, upate the ranges
static int add_data_block_to_file(mounted_data *mt, inode *inode, uint32_t *absolute_block_no) {
    int err;
    int use_indirect_block;

    err = add_block_to_array_of_ranges(mt, 
        inode->ranges, RANGES_IN_INODE, 
        inode->indirect_ranges_block_no != 0, &use_indirect_block, 
        absolute_block_no);
    if (err == OK && !use_indirect_block) {
        inode->allocated_blocks++;
        return OK;
    }
    
    // so either we failed, or we need the indirect block
    if (!use_indirect_block)
        return err;
    
    // load or create the indirect block?
    if (inode->indirect_ranges_block_no == 0) {
        err = find_a_free_block(mt, &inode->indirect_ranges_block_no);
        if (err != OK) return err;
        mark_block_used(mt, inode->indirect_ranges_block_no);
        err = cached_wipe(mt->cache, inode->indirect_ranges_block_no);
        if (err != OK) return err;
    } else {
        err = cached_read(mt->cache, 
            inode->indirect_ranges_block_no, 0,
            mt->scratch_block_buffer, mt->superblock->block_size_in_bytes);
        if (err != OK) return err;
    }

    err = add_block_to_array_of_ranges(mt, 
        (block_range *)mt->scratch_block_buffer, 
        mt->superblock->block_size_in_bytes / sizeof(block_range), 
        0, // no fallback exists
        &use_indirect_block, // actualy, we don't care
        absolute_block_no);
    if (err != OK) return err;

    // write back the changes
    err = cached_write(mt->cache, 
        inode->indirect_ranges_block_no, 0,
        mt->scratch_block_buffer,
        mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    // finally! this was a lot!
    inode->allocated_blocks++;
    return OK;
}
