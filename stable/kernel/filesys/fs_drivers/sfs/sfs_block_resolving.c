#include "sfs_internal.h"


static inline bool _range_contains_index(block_range *range, size_t *block_index, block_no_t *data_block_no) {
    if ((*block_index) < range->blocks_count) {  // note: LT not LE
        // this is the range, calculate final block no
        (*data_block_no) = range->first_block_no + (*block_index);
        return true;
    } else {
        // deplete index for next iteration
        (*block_index) -= range->blocks_count;
        return false;
    }
}

static inline bool _find_block_in_range_array(block_range *arr, int arr_items, size_t *block_index, block_no_t *data_block_no) {
    for (int i = 0; i < arr_items; i++) {
        if (arr[i].first_block_no == 0)
            break;
        if (_range_contains_index(&arr[i], block_index, data_block_no))
            return true;
    }
    return false;
}

static inline error_t _find_block_index_in_indirect_block(sfs_mount_data *md, block_no_t indirect_block_no, bool *found, size_t *block_index, block_no_t *data_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, indirect_block_no, md->generic_block_buffer);
    if (err) return traceable(err);
    int ranges_in_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    (*found) = _find_block_in_range_array(
        (block_range *)md->generic_block_buffer,
        ranges_in_block,
        block_index,
        data_block_no
    );
    return OK;
}

static inline error_t _find_block_index_in_double_indirect_block(sfs_mount_data *md, block_no_t dbl_ind_block_no, bool *found, size_t *block_index, block_no_t *data_block_no) {
    error_t err = md->block_cache->ops->read(md->block_cache, dbl_ind_block_no, md->generic_block_buffer);
    if (err) return traceable(err);

    int ranges_in_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    block_range *arr = (block_range *)md->generic_block_buffer;
    for (int i = 0; i < ranges_in_block; i++) {
        block_range range = arr[i];
        if (range.first_block_no == 0)
            break;

        block_no_t indirect_block_no = range.first_block_no;
        uint32_t count = range.blocks_count;
        while (count-- > 0) {
            err = _find_block_index_in_indirect_block(md, indirect_block_no, found, block_index, data_block_no);
            if (err) return traceable(err);
            if (*found) return OK;

            indirect_block_no++;
        }
    }
    return OK;
}

error_t sfs_node_resolve_data_block(sfs_mount_data *md, stored_inode *sin, block_no_t block_index, block_no_t *data_block_no) {
    error_t err;
    bool found = false;

    if (_find_block_in_range_array(sin->ranges, RANGES_IN_INODE, &block_index, data_block_no)) {
        return OK;
    }
    if (sin->indirect_ranges_block_no) {
        err = _find_block_index_in_indirect_block(md, sin->indirect_ranges_block_no, &found, &block_index, data_block_no);
        if (err) return traceable(err);
        if (found) return OK;
    }
    if (sin->double_indirect_block_no) {
        err = _find_block_index_in_double_indirect_block(md, sin->double_indirect_block_no, &found, &block_index, data_block_no);
        if (err) return traceable(err);
        if (found) return OK;
    }

    return traceable(ERR_NOT_FOUND);
}

