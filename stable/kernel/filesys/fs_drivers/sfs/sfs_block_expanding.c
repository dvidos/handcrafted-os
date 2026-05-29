#include "sfs_internal.h"
#include "../../../utils/assert.h"



static inline int _find_last_used_range(block_range *arr, int arr_count) {
    for (int i = arr_count - 1; i >= 0; i--)
        if (arr[i].first_block_no > 0)
            return i;
    return -1;
}

error_t sfs_expand_range_array(sfs_mount_data *md, block_range *arr, int arr_count, bool *overflow, block_no_t *new_block_no) {

    block_no_t num;
    *overflow = false;
    int index = _find_last_used_range(arr, arr_count);

    // if no range found, create the first one
    if (index == -1) {
        if (!md->block_bitmap->ops->find_next_free(md->block_bitmap, &num))
            return traceable(ERR_NO_SPACE_LEFT);
        md->block_bitmap->ops->mark_used(md->block_bitmap, num);
        arr[0].first_block_no = num;
        arr[0].blocks_count = 1;
        md->block_cache->ops->fill(md->block_cache, num, 0);
        *new_block_no = num;
        *overflow = false;
        return OK;
    }

    // there is at least one range, see if we can expand it
    block_no_t subsequent_block = arr[index].first_block_no + arr[index].blocks_count;
    if (md->block_bitmap->ops->is_free(md->block_bitmap, subsequent_block)) {
        md->block_bitmap->ops->mark_used(md->block_bitmap, subsequent_block);
        arr[index].blocks_count += 1;
        md->block_cache->ops->fill(md->block_cache, subsequent_block, 0);
        *new_block_no = subsequent_block;
        *overflow = false;
        return OK;
    }

    // we cannot expand it, see if we can create new range
    if (index < arr_count - 1) {
        if (!md->block_bitmap->ops->find_next_free(md->block_bitmap, &num))
            return traceable(ERR_NO_SPACE_LEFT);
        md->block_bitmap->ops->mark_used(md->block_bitmap, num);
        arr[index + 1].first_block_no = num;
        arr[index + 1].blocks_count = 1;
        md->block_cache->ops->fill(md->block_cache, num, 0);
        *new_block_no = num;
        *overflow = false;
        return OK;
    }

    // we have overflown, let caller escalate
    *overflow = true;
    return OK;
}

error_t sfs_expand_range_block(sfs_mount_data *md, block_no_t indirect_block_no, bool *overflow, block_no_t *new_block_no) {
    block_range *ranges_arr;

    error_t err = md->block_cache->ops->get(md->block_cache, indirect_block_no, (void **)&ranges_arr);
    if (err) return traceable(err);

    *overflow = false;
    int ranges_per_block = md->superblock->block_size_in_bytes / sizeof(block_range);
    err = sfs_expand_range_array(md, ranges_arr, ranges_per_block, overflow, new_block_no);
    if (err) return traceable(err);

    // if overflown, we cannot do anything more
    if (*overflow)
        return OK;

    // since we succeeded, mark the (indirect) block dirty
    md->block_cache->ops->mark_dirty(md->block_cache, indirect_block_no);
    return OK;
}

error_t sfs_expand_dbl_indirect_block(sfs_mount_data *md, block_no_t dbl_indirect_block_no, bool *overflow, block_no_t *new_block_no) {
    block_range *dbl_ind_ranges_arr;
    block_no_t indirect_block_no = 0;
    block_no_t data_block_no = 0;
    error_t err;
    int ranges_per_block = md->superblock->block_size_in_bytes / sizeof(block_range);

    // use cases:
    // - if no dbl ranges at all, create the first, using a new indirect block
    // - try expanding current last-indirect-block
    // - failing that, try expanding the last dbl range, grab new indirect block
    // - failing that, try creating new dbl range, grab new indirect block
    // - failing that, it means we run out of dbl ranges and could not expand the last indirect. FAIL.
    err = md->block_cache->ops->get(md->block_cache, dbl_indirect_block_no, (void **)&dbl_ind_ranges_arr);
    if (err) return traceable(err);

    // find last block of last range, 
    int index = _find_last_used_range(dbl_ind_ranges_arr, ranges_per_block);
    if (index == -1) {
        // expand the double indirct with a single indirect
        // we should expect no overflow here and block_no to have a valid value
        err = sfs_expand_range_array(md, dbl_ind_ranges_arr, ranges_per_block, overflow, &indirect_block_no);
        if (err) return traceable(err);
        if (*overflow) panic("overflow should be false!");
        if (!indirect_block_no) panic("block_no should have value!");

        // expand the single indirect as well, to get a new data block
        err = sfs_expand_range_block(md, indirect_block_no, overflow, &data_block_no);
        if (err) return traceable(err);
        if (*overflow) panic("overflow should be false!");
        if (!data_block_no) panic("block_no should have value!");

        return OK;
    }

    // we already have a range with an indirect block in here, see if can expand that.
    block_range *last_range = &dbl_ind_ranges_arr[index];
    indirect_block_no = last_range->first_block_no + last_range->blocks_count - 1;
    err = sfs_expand_range_block(md, indirect_block_no, overflow, new_block_no);
    if (err) return traceable(err);
    if (!(*overflow)) return OK;

    // we had an indirect block, but failed to expand. let's add a new range of indirect blocks.
    err = sfs_expand_range_block(md, dbl_indirect_block_no, overflow, &indirect_block_no);
    if (err) return traceable(err);
    if (*overflow) return traceable(ERR_NO_SPACE_LEFT); // it means we could not expand the double indirect...

    // we expanded the double indirect, let's initialize a range in the single indirect
    err = sfs_expand_range_block(md, indirect_block_no, overflow, new_block_no);    
    if (err) return traceable(err);
    if (*overflow) panic("overflow should be false!");
    if (!(*new_block_no)) panic("block_no should have value!");

    // finally we made it.
    return OK;
}

