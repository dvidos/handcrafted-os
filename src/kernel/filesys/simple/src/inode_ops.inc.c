#include "internal.h"


static int inode_recalculate_allocated_blocks(mounted_data *mt, stored_inode *node, uint32_t *block_count) {
    uint32_t count = 0;

    if (node->indirect_ranges_block_no != 0) {
        int err = bcache_read(mt->cache, node->indirect_ranges_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
        if (err != OK) return err;

        int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
        for (int i = 0; i < ranges_in_block; i++)
            count += ((block_range *)(mt->generic_block_buffer + i * sizeof(block_range)))->blocks_count;
    }

    for (int i = 0; i < RANGES_IN_INODE; i++)
        count += node->ranges[i].blocks_count;

    *block_count = count;
    return OK;
}

// -----------------------------------------------------------------------------

static int inode_read_file_bytes(mounted_data *mt, cached_inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos >= n->inode.file_size)
        return ERR_END_OF_FILE;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    uint32_t max_chunk_len = 0;
    uint32_t chunk_length = 0;
    int bytes_read = 0;

    // now we know the absolute block, we can read it (at most till EOF)
    while (length > 0 && file_pos < n->inode.file_size) {
        err = inode_resolve_block(mt, n, block_index, &disk_block_no);
        if (err != OK) return err;
        
        uint32_t bytes_till_block_end = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t bytes_till_file_end = n->inode.file_size - file_pos;
        max_chunk_len = min(bytes_till_block_end, bytes_till_file_end);
        chunk_length = at_most(length, max_chunk_len);

        err = bcache_read(mt->cache, disk_block_no, block_offset, data, chunk_length);
        if (err != OK) return err;

        // prepare for next block
        data += chunk_length;
        length -= chunk_length;
        bytes_read += chunk_length;
        file_pos += chunk_length;
        block_offset += chunk_length;

        if (bytes_till_block_end > 0 && chunk_length == bytes_till_block_end) {
            block_index += 1;
            block_offset = 0;
        }
    }

    return bytes_read;
}

static int inode_write_file_bytes(mounted_data *mt, cached_inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos > n->inode.file_size)
        file_pos = n->inode.file_size;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    uint32_t max_chunk_len = 0;
    uint32_t chunk_length = 0;
    int bytes_written = 0;
    int writing_at_eof = 0;

    // now we know the absolute block, we can read it
    while (length > 0) {
        writing_at_eof = (file_pos >= n->inode.file_size);
        
        // there may be space enough in the allocated blocks or we need to add a block
        if (block_index >= n->inode.allocated_blocks) {
            err = inode_extend_file_blocks(mt, n, &disk_block_no);
            if (err != OK) return err;
        } else {
            err = inode_resolve_block(mt, n, block_index, &disk_block_no);
            if (err != OK) return err;
        }

        // we need to break the chunk either at (1) block boundary, or at (2) file boundary
        uint32_t bytes_till_block_end = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t bytes_till_file_end = n->inode.file_size - file_pos;
        if (writing_at_eof) {
            max_chunk_len = bytes_till_block_end;
        } else {
            // write till whatever finished first, so we have correct length housekeeping
            max_chunk_len = min(bytes_till_block_end, bytes_till_file_end);
        }
        chunk_length = at_most(length, max_chunk_len);

        err = bcache_write(mt->cache, disk_block_no, block_offset, data, chunk_length);
        if (err != OK) return err;

        // prepare for next block
        data += chunk_length;
        length -= chunk_length;
        bytes_written += chunk_length;
        file_pos += chunk_length;
        block_offset += chunk_length;

        if (bytes_till_block_end > 0 && chunk_length == bytes_till_block_end) {
            // we moved past end of block
            block_index += 1;
            block_offset = 0;
        }
        if (writing_at_eof) {
            // we extended the file
            n->inode.file_size += chunk_length;
        }
    }

    if (bytes_written > 0) {
        n->inode.modified_at = mt->clock->get_seconds_since_epoch(mt->clock);
        n->is_dirty = 1;
    }

    return bytes_written;
}

static int inode_read_file_rec(mounted_data *mt, cached_inode *n, uint32_t rec_size, uint32_t rec_no, void *rec) {
    int bytes = inode_read_file_bytes(mt, n, 
        rec_size * rec_no,
        rec,
        rec_size
    );
    if (bytes == rec_size) return OK;
    if (bytes < 0)         return bytes;
    return ERR_CORRUPTION_DETECTED;
}

static int inode_write_file_rec(mounted_data *mt, cached_inode *n, uint32_t rec_size, uint32_t rec_no, void *rec) {
    int bytes = inode_write_file_bytes(mt, n, 
        rec_size * rec_no,
        rec,
        rec_size
    );
    if (bytes == rec_size) return OK;
    if (bytes < 0)         return bytes;
    return ERR_CORRUPTION_DETECTED;
}

static int inode_truncate_file_bytes(mounted_data *mt, cached_inode *n) {
    // release all blocks, reset ranges to zero.
    int err;

    if (n->inode.indirect_ranges_block_no != 0) {
        err = bcache_read(mt->cache, n->inode.indirect_ranges_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
        if (err != OK) return err;

        // release all pointed blocks
        int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
        range_array_release_blocks(mt, (block_range *)mt->generic_block_buffer, ranges_in_block);

        // release indirect as well
        bitmap_mark_block_free(mt->bitmap, n->inode.indirect_ranges_block_no);
        n->inode.indirect_ranges_block_no = 0;
    }

    // then the internal ranges
    range_array_release_blocks(mt, n->inode.ranges, RANGES_IN_INODE);

    // finally, reset inode file size to zero
    n->inode.modified_at = mt->clock->get_seconds_since_epoch(mt->clock);
    n->inode.allocated_blocks = 0;
    n->inode.file_size = 0;
    n->is_dirty = 1;

    return OK;
}

// given an inode, it resolves the relative block index in the file, into the block_no on the disk
static int inode_resolve_block(mounted_data *mt, cached_inode *node, uint32_t block_index_in_file, uint32_t *absolute_block_no) {
    if (block_index_in_file >= node->inode.allocated_blocks)
        return ERR_OUT_OF_BOUNDS;

    // first, try the inline ranges
    for (int i = 0; i < RANGES_IN_INODE; i++) {
        if (is_range_empty(&node->inode.ranges[i]))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&node->inode.ranges[i], &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if there is no extra ranges block, we cannot find it
    if (node->inode.indirect_ranges_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    
    int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    block_range range;
    for (int i = 0; i < ranges_in_block; i++) {
        int err = bcache_read(mt->cache, node->inode.indirect_ranges_block_no, sizeof(block_range) * i, (void *)&range, sizeof(block_range));
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
static int inode_extend_file_blocks(mounted_data *mt, cached_inode *node, uint32_t *absolute_block_no) {
    int err;
    int use_indirect_block;

    // if there is indirect block, no point in extending the ranges
    if (node->inode.indirect_ranges_block_no == 0) {
        err = add_block_to_array_of_ranges(mt, 
            node->inode.ranges, RANGES_IN_INODE, 
            1, &use_indirect_block, 
            absolute_block_no);
        if (err == OK && !use_indirect_block) {
            // we managed to allocate
            node->inode.allocated_blocks++;
            return OK;
        }
        
        // so either we failed, or we need the indirect block
        if (!use_indirect_block)
            return ERR_RESOURCES_EXHAUSTED;
    }
    
    // if block not available, allocate one
    if (node->inode.indirect_ranges_block_no == 0) {
        err = bitmap_find_free_block(mt->bitmap, &node->inode.indirect_ranges_block_no);
        if (err != OK) return err;
        bitmap_mark_block_used(mt->bitmap, node->inode.indirect_ranges_block_no);
        err = bcache_wipe(mt->cache, node->inode.indirect_ranges_block_no);
        if (err != OK) return err;
    }

    // read, so we can iterate on it.
    err = bcache_read(mt->cache, 
        node->inode.indirect_ranges_block_no, 0,
        mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    err = add_block_to_array_of_ranges(mt, 
        (block_range *)mt->generic_block_buffer, 
        mt->superblock->block_size_in_bytes / sizeof(block_range), 
        0, // no fallback exists
        &use_indirect_block, // actualy, we don't care
        absolute_block_no);
    if (err != OK) return err;

    // write back the changes
    err = bcache_write(mt->cache, 
        node->inode.indirect_ranges_block_no, 0,
        mt->generic_block_buffer,
        mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    // finally! this was a lot!
    node->inode.allocated_blocks++;
    node->is_dirty = 1;
    return OK;
}

// -----------------------------------------------------------------------------

static int inode_db_rec_count(mounted_data *mt) {
    return mt->cached_inodes_db_inode->inode.file_size / sizeof(stored_inode);
}

static int inode_db_load(mounted_data *mt, uint32_t inode_id, stored_inode *node) {
    if (inode_id == INODE_DB_INODE_ID) {
        memcpy(node, &mt->superblock->inodes_db_inode, sizeof(stored_inode));
        return OK;
    } else if (inode_id == ROOT_DIR_INODE_ID) {
        memcpy(node, &mt->superblock->root_dir_inode, sizeof(stored_inode));
        return OK;
    }

    int err = inode_read_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, node);
    if (err != OK) return err;

    return OK;
}

static stored_inode inode_prepare(clock_device *clock, int for_dir) {
    stored_inode node;

    memset(&node, 0, sizeof(stored_inode));
    if (for_dir)
        node.is_dir = 1;
    else
        node.is_file = 1;
    node.is_used = 1;
    node.modified_at = clock->get_seconds_since_epoch(clock);

    return node;
}

static int inode_db_append(mounted_data *mt, stored_inode *node, uint32_t *inode_id) {
    int recs = inode_db_rec_count(mt);
    if (recs >= UINT32_MAX - 2)
        return ERR_RESOURCES_EXHAUSTED; // try reusing deleted inodes?

    // this should have a lock somehow (race conditions)
    int rec_no = recs;
    int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), rec_no, node);
    if (err != OK) return err;

    *inode_id = rec_no;
    mt->superblock->inodes_db_rec_count = recs + 1;
    return OK;
}

static int inode_db_update(mounted_data *mt, uint32_t inode_id, stored_inode *node) {
    if (inode_id == INODE_DB_INODE_ID) {
        memcpy(&mt->superblock->inodes_db_inode, node, sizeof(stored_inode));
        return OK;
    } else if (inode_id == ROOT_DIR_INODE_ID) {
        memcpy(&mt->superblock->root_dir_inode, node, sizeof(stored_inode));
        return OK;
    }

    int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, node);
    if (err != OK) return err;

    return OK;
}

static int inode_db_delete(mounted_data *mt, uint32_t inode_id) {
    if (inode_id == INODE_DB_INODE_ID || inode_id == ROOT_DIR_INODE_ID)
        return ERR_NOT_PERMITTED;
    
    int recs = inode_db_rec_count(mt);
    if (inode_id == recs - 1) {
        // shorten the file, but keep blocks for simplicity
        mt->cached_inodes_db_inode->inode.file_size = (recs - 1) * sizeof(stored_inode);
        mt->superblock->inodes_db_rec_count = recs - 1;

    } else {
        stored_inode blank;
        memset(&blank, 0, sizeof(stored_inode));
        int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, &blank);
        if (err != OK) return err;
    }

    return OK;
}

static void inode_dump_debug_info(mounted_data *mt, const char *title, stored_inode *n) {
    printf("%s [U:%d, F:%d, D:%d, FileSz=%d, AlocBlks=%d, Ranges=(%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d), Indirect=%d", 
        title,
        n->is_used,
        n->is_file,
        n->is_dir,
        n->file_size,
        n->allocated_blocks,
        n->ranges[0].first_block_no, n->ranges[0].blocks_count,
        n->ranges[1].first_block_no, n->ranges[1].blocks_count,
        n->ranges[2].first_block_no, n->ranges[2].blocks_count,
        n->ranges[3].first_block_no, n->ranges[3].blocks_count,
        n->ranges[4].first_block_no, n->ranges[4].blocks_count,
        n->ranges[5].first_block_no, n->ranges[5].blocks_count,
        n->indirect_ranges_block_no
    );

    if (n->indirect_ranges_block_no != 0) {
        printf("=(");
        int err = bcache_read(mt->cache, n->indirect_ranges_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
        if (err == OK) {
            int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
            for (int i = 0; i < ranges_in_block; i++) {
                block_range *r = (block_range *)(mt->generic_block_buffer + i * sizeof(block_range));
                printf("%d:%d, ", r->first_block_no, r->blocks_count);
                if (i % 20 == 0) printf("\n%*s", 30, "");
            }
        }
        printf(")");
    }

    printf("]\n");
}

