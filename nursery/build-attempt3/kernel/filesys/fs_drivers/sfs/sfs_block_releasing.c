#include "sfs_internal.h"
#include "../../../utils/assert.h"



static void _release_data_blocks_in_range_arr(sfs_mount_data *md, block_range *arr, int range_count) {
    for (int i = 0; i < range_count; i++) {
        block_range range = arr[i];
        if (range.first_block_no == 0)
            break;
        
        block_no_t block_no = range.first_block_no;
        uint32_t count = range.blocks_count;
        while (count-- > 0) {
            md->block_bitmap->ops->mark_free(md->block_bitmap, block_no);
            block_no++;
        }
    }
}

static error_t _release_indirect_and_data_blocks(sfs_mount_data *md, block_no_t indirect_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, indirect_block_no, md->generic_block_buffer);
    if (err) return err;

    int ranges_in_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    _release_data_blocks_in_range_arr(md, (block_range *)md->generic_block_buffer, ranges_in_block);

    // we need to also release the indirect block itself
    md->block_bitmap->ops->mark_free(md->block_bitmap, indirect_block_no);
    return OK;
}

static error_t _release_dbl_indirect_and_indirect_blocks(sfs_mount_data *md, block_no_t dbl_indirect_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, dbl_indirect_block_no, md->generic_block_buffer);
    if (err) return err;

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

error_t sfs_node_release_all_data_blocks(sfs_mount_data *md, inode_no_t inode_num) {
    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, inode_num, (void **)&sin);
    if (err) return err;

    _release_data_blocks_in_range_arr(md, sin->ranges, RANGES_IN_INODE);
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
    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_num);
    return OK;
}
