#include "sfs_internal.h"
#include "../../../klib/string.h"




error_t sfs_node_expand_data_blocks(sfs_mount_data *md, stored_inode *sin) {
    log_trace("sfs_node_expand_data_blocks(node=%p)", sin);
    // first find the last range (node, indirect, dbl_indirect) and always try to expand on that
    // if that doesn't work, try allocating new range, wherever this will be
    // if there is no space on the last doubly indirected block, then return traceable(ERR_NO_SPACE
    // perform conservative strategy of expanding last range

    error_t err;
    bool overflow = false;
    block_no_t block_no;

    if (sin->double_indirect_block_no > 0) {
        // we need to enlarge this
        err = sfs_expand_dbl_indirect_block(md, sin->double_indirect_block_no, &overflow, &block_no);
        if (err) return traceable(err);

    } else if (sin->indirect_ranges_block_no > 0) {
        // we need to enlarge this, fallback onto the double
        err = sfs_expand_range_block(md, sin->indirect_ranges_block_no, &overflow, &block_no);
        if (err) return traceable(err);

        if (overflow) {
            // we need to grab a new double indirect!
            if (!md->block_bitmap->ops->find_next_free(md->block_bitmap, &block_no))
                return traceable(ERR_NO_SPACE_LEFT);
            sin->double_indirect_block_no = block_no;
            md->block_bitmap->ops->mark_used(md->block_bitmap, sin->double_indirect_block_no);
            md->block_cache->ops->fill(md->block_cache, sin->double_indirect_block_no, 0);

            err = sfs_expand_dbl_indirect_block(md, sin->double_indirect_block_no, &overflow, &block_no);
            if (err) return traceable(err);
        }

    } else {
        // we need to enlarge built-in ranges, fallback to single, then double indirect.
        err = sfs_expand_range_array(md, sin->ranges, RANGES_IN_INODE, &overflow, &block_no);
        if (err) return traceable(err);

        if (overflow) {
            // we need to grab a new indirect block
            if (!md->block_bitmap->ops->find_next_free(md->block_bitmap, &block_no))
                return traceable(ERR_NO_SPACE_LEFT);
            sin->indirect_ranges_block_no = block_no;
            md->block_bitmap->ops->mark_used(md->block_bitmap, sin->indirect_ranges_block_no);
            md->block_cache->ops->fill(md->block_cache, sin->indirect_ranges_block_no, 0);

            err = sfs_expand_range_block(md, sin->indirect_ranges_block_no, &overflow, &block_no);
            if (err) return traceable(err);
            // there should be no reason for fallback, single indirct should be ok for one block
        }
    }

    return overflow ? ERR_NO_SPACE_LEFT : OK;
}

// ---------------------------------------------------------------------

static error_t _release_indirect_and_data_blocks(sfs_mount_data *md, block_no_t indirect_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, indirect_block_no, md->generic_block_buffer);
    if (err) return traceable(err);

    int ranges_in_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    sfs_release_block_range_array(md, (block_range *)md->generic_block_buffer, ranges_in_block);

    // we need to also release the indirect block itself
    md->block_bitmap->ops->mark_free(md->block_bitmap, indirect_block_no);
    return OK;
}

static error_t _release_dbl_indirect_and_indirect_blocks(sfs_mount_data *md, block_no_t dbl_indirect_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, dbl_indirect_block_no, md->generic_block_buffer);
    if (err) return traceable(err);

    int ranges_in_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    block_range *ranges_arr = (block_range *)md->generic_block_buffer;
    for (int i = 0; i < ranges_in_block; i++) {
        block_range *range = &ranges_arr[i];
        if (range->first_block_no == 0)
            break;

        block_no_t indirect_block_no = range->first_block_no;
        uint32_t count = range->blocks_count;
        while (count-- > 0) {
            _release_indirect_and_data_blocks(md, indirect_block_no);
            indirect_block_no++;
        }
    }

    md->block_bitmap->ops->mark_free(md->block_bitmap, dbl_indirect_block_no);
    return OK;
}

error_t sfs_node_release_all_data_blocks(sfs_mount_data *md, stored_inode *sin) {
    log_trace("sfs_node_release_all_data_blocks(node=%p)", sin);

    sfs_release_block_range_array(md, sin->ranges, RANGES_IN_INODE);
    if (sin->indirect_ranges_block_no) {
        _release_indirect_and_data_blocks(md, sin->indirect_ranges_block_no);
        sin->indirect_ranges_block_no = 0;
    }
    if (sin->double_indirect_block_no) {
        _release_dbl_indirect_and_indirect_blocks(md, sin->double_indirect_block_no);
        sin->double_indirect_block_no = 0;
    }

    sin->allocated_blocks = 0;
    sin->file_size = 0;
    return OK;
}

error_t sfs_node_num_release_all_data_blocks(sfs_mount_data *md, inode_no_t inode_num) {
    log_trace("sfs_node_num_release_all_data_blocks(node_num=%u)", inode_num);

    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, inode_num, (void **)&sin);
    if (err) return traceable(err);

    sfs_node_release_all_data_blocks(md, sin);

    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_num);
    return OK;
}

