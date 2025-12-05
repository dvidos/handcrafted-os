#include "internal.h"


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
        err = inode_resolve_index(mt, n, block_index, &disk_block_no);
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
            err = inode_expand_blocks(mt, n, &disk_block_no);
            if (err != OK) return err;
        } else {
            err = inode_resolve_index(mt, n, block_index, &disk_block_no);
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
        range_array_release_blocks((block_range *)mt->generic_block_buffer, ranges_in_block, mt->bitmap);

        // release indirect as well
        bitmap_mark_block_free(mt->bitmap, n->inode.indirect_ranges_block_no);
        n->inode.indirect_ranges_block_no = 0;
    }

    // then the internal ranges
    range_array_release_blocks(n->inode.ranges, RANGES_IN_INODE, mt->bitmap);

    // finally, reset inode file size to zero
    n->inode.modified_at = mt->clock->get_seconds_since_epoch(mt->clock);
    n->inode.allocated_blocks = 0;
    n->inode.file_size = 0;
    n->is_dirty = 1;

    return OK;
}

// given an inode, it resolves the relative block index in the file, into the block_no on the disk
static int inode_resolve_index(mounted_data *mt, cached_inode *node, uint32_t block_index_in_file, uint32_t *absolute_block_no) {
    if (block_index_in_file >= node->inode.allocated_blocks)
        return ERR_OUT_OF_BOUNDS;

    // notice that ERR_NOT_FOUND here means we continue to next level of indirection...
    int err = range_array_resolve_index(node->inode.ranges, RANGES_IN_INODE, &block_index_in_file, absolute_block_no);
    if (err == OK) return OK;
    if (err != ERR_NOT_FOUND) return err;

    // fall to next level
    if (node->inode.indirect_ranges_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    err = range_block_resolve_indirect_index(mt, node->inode.indirect_ranges_block_no, &block_index_in_file, absolute_block_no);
    if (err == OK) return OK;
    if (err != ERR_NOT_FOUND) return err;
    
    // fall to next level
    if (node->inode.double_indirect_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    err = range_block_resolve_dbl_indirect_index(mt, node->inode.double_indirect_block_no, &block_index_in_file, absolute_block_no);
    if (err == OK) return OK;
    if (err != ERR_NOT_FOUND) return err;

    return ERR_OUT_OF_BOUNDS;
}

// allocate and add a data block at the end of a file, upate the ranges
static int inode_expand_blocks(mounted_data *mt, cached_inode *node, uint32_t *new_block_no) {

    uint32_t dbl_indirect_block_no;
    uint32_t indirect_block_no;
    int overflown;
    int err;
    
    if (node->inode.double_indirect_block_no != 0) {
        // find indirection and expand indirect block
        err = range_block_last_block_no(mt, node->inode.double_indirect_block_no, &indirect_block_no);
        if (err != OK) return err;
        err = range_block_expand(mt, indirect_block_no, new_block_no, &overflown);
        if (err != OK) return err;

        if (overflown) {
            // fallback to expanding the double indirect, initialize new indirect
            err = range_block_expand(mt, node->inode.double_indirect_block_no, &indirect_block_no, &overflown);
            if (err != OK) return err;
            if (overflown) return ERR_RESOURCES_EXHAUSTED;  // if double indirect cannot be expanded, we failed.
            err = range_block_initialize(mt, indirect_block_no, new_block_no);
            if (err != OK) return err;
        }

    } else if (node->inode.indirect_ranges_block_no != 0) {
        // attempt to expand the indirect
        err = range_block_expand(mt, node->inode.indirect_ranges_block_no, new_block_no, &overflown);
        if (err != OK) return err;

        if (overflown) {
            // fallback to creating a double indirect with its first indirect
            err = bitmap_find_free_block(mt->bitmap, &dbl_indirect_block_no);
            if (err != OK) return err;
            bitmap_mark_block_used(mt->bitmap, dbl_indirect_block_no);
            err = range_block_initialize(mt, dbl_indirect_block_no, &indirect_block_no);
            if (err != OK) return err;
            err = range_block_initialize(mt, indirect_block_no, new_block_no);
            if (err != OK) return err;
            node->inode.double_indirect_block_no = dbl_indirect_block_no;
            node->is_dirty = 1;
        }

    } else {
        // no double or single indirect, try expanding inline ranges
        err = range_array_expand(node->inode.ranges, RANGES_IN_INODE, mt->bitmap, new_block_no, &overflown);
        if (err != OK) return err;

        if (overflown) {
            // fallback to create the indirect block
            err = bitmap_find_free_block(mt->bitmap, &indirect_block_no);
            if (err != OK) return err;
            bitmap_mark_block_used(mt->bitmap, indirect_block_no);
            err = range_block_initialize(mt, indirect_block_no, new_block_no);
            if (err != OK) return err;
            node->inode.indirect_ranges_block_no = indirect_block_no;
            node->is_dirty = 1;
        }
    }

    // now matter how we ended up to a new_block_no, we did, so wipe clean!
    err = bcache_wipe(mt->cache, *new_block_no);
    if (err != OK) return err;
    node->inode.allocated_blocks++;
    node->is_dirty = 1;

    return OK;
}

// -----------------------------------------------------------------------------

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

