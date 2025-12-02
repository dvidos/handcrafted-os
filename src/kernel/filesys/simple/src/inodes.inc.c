#include "internal.h"


static int inode_recalculate_allocated_blocks(mounted_data *mt, inode *node, uint32_t *block_count) {
    uint32_t count = 0;

    if (node->indirect_ranges_block_no != 0) {
        int err = cached_read(mt->cache, node->indirect_ranges_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
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

static int inode_read_file_bytes(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos >= n->file_size)
        return ERR_END_OF_FILE;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    uint32_t max_chunk_len = 0;
    uint32_t chunk_length = 0;
    int bytes_read = 0;

    // now we know the absolute block, we can read it (at most till EOF)
    while (length > 0 && file_pos < n->file_size) {
        err = find_block_no_from_file_block_index(mt, n, block_index, &disk_block_no);
        if (err != OK) return err;
        
        uint32_t bytes_till_block_end = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t bytes_till_file_end = n->file_size - file_pos;
        max_chunk_len = min(bytes_till_block_end, bytes_till_file_end);
        chunk_length = at_most(length, max_chunk_len);

        err = cached_read(mt->cache, disk_block_no, block_offset, data, chunk_length);
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

static int inode_write_file_bytes(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos > n->file_size)
        file_pos = n->file_size;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    uint32_t max_chunk_len = 0;
    uint32_t chunk_length = 0;
    int bytes_written = 0;
    int writing_at_eof = 0;

    // now we know the absolute block, we can read it
    while (length > 0) {
        writing_at_eof = (file_pos >= n->file_size);
        
        // there may be space enough in the allocated blocks or we need to add a block
        if (block_index >= n->allocated_blocks) {
            err = add_data_block_to_file(mt, n, &disk_block_no);
            if (err != OK) return err;
        } else {
            err = find_block_no_from_file_block_index(mt, n, block_index, &disk_block_no);
            if (err != OK) return err;
        }

        // we need to break the chunk either at (1) block boundary, or at (2) file boundary
        uint32_t bytes_till_block_end = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t bytes_till_file_end = n->file_size - file_pos;
        if (writing_at_eof) {
            max_chunk_len = bytes_till_block_end;
        } else {
            // write till whatever finished first, so we have correct length housekeeping
            max_chunk_len = min(bytes_till_block_end, bytes_till_file_end);
        }
        chunk_length = at_most(length, max_chunk_len);

        err = cached_write(mt->cache, disk_block_no, block_offset, data, chunk_length);
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
            n->file_size += chunk_length;
        }
    }

    if (bytes_written > 0)
        n->modified_at = mt->clock->get_seconds_since_epoch(mt->clock);

    return bytes_written;
}

static int inode_truncate_file(mounted_data *mt, inode *n) {
    // release all blocks, reset ranges to zero.
    int err;

    if (n->indirect_ranges_block_no != 0) {
        err = cached_read(mt->cache, n->indirect_ranges_block_no, 0, mt->generic_block_buffer, mt->superblock->block_size_in_bytes);
        if (err != OK) return err;

        // release all pointed blocks
        int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
        range_array_release_blocks(mt, (block_range *)mt->generic_block_buffer, ranges_in_block);

        // release indirect as well
        mark_block_free(mt, n->indirect_ranges_block_no);
        n->indirect_ranges_block_no = 0;
    }

    // then the internal ranges
    range_array_release_blocks(mt, n->ranges, RANGES_IN_INODE);

    // finally, reset inode file size to zero
    n->modified_at = mt->clock->get_seconds_since_epoch(mt->clock);
    n->allocated_blocks = 0;
    n->file_size = 0;

    return OK;
}

// -----------------------------------------------------------------------------

static int inode_load(mounted_data *mt, uint32_t inode_id, inode *node) {
    int bytes = inode_read_file_bytes(mt, &mt->superblock->inodes_db_inode,
        inode_id * sizeof(inode), 
        node,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

static int inode_allocate(mounted_data *mt, int is_dir, inode *node, uint32_t *inode_id) {
    memset(node, 0, sizeof(inode));
    if (is_dir) {
        node->is_dir = 1;
    } else {
        node->is_file = 1;
    }
    node->is_used = 1;
    node->modified_at = mt->clock->get_seconds_since_epoch(mt->clock);

    // this should have a lock somehow
    *inode_id = mt->superblock->inodes_db_rec_count;
    int bytes = inode_write_file_bytes(mt, &mt->superblock->inodes_db_inode, *inode_id * sizeof(inode), node, sizeof(inode));
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    mt->superblock->inodes_db_rec_count += 1;
    return OK;
}

static int inode_persist(mounted_data *mt, uint32_t inode_id, inode *node) {
    if (inode_id == ROOT_DIR_INODE_ID) {
        memcpy(&mt->superblock->root_dir_inode, node, sizeof(inode));
        return OK;
    }

    int bytes = inode_write_file_bytes(mt, &mt->superblock->inodes_db_inode, 
        inode_id * sizeof(inode),
        node,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

static int inode_delete(mounted_data *mt, uint32_t inode_id) {
    if (inode_id == ROOT_DIR_INODE_ID)
        return ERR_NOT_PERMITTED;
    
    inode blank;
    memset(&blank, 0, sizeof(inode));

    int bytes = inode_write_file_bytes(mt, &mt->superblock->inodes_db_inode,
        inode_id * sizeof(inode),
        &blank,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

static void inode_dump_debug_info(const char *title, inode *n) {
    printf("%s [U:%d, F:%d, D:%d, FileSz=%d, AlocBlks=%d, Ranges=(%d:%d,%d:%d,%d:%d,%d:%d,%d:%d,%d:%d), Indirect=%d]\n", 
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
}

