#include "sfs_internal.h"
#include "../../../utils/assert.h"



void sfs_release_block(sfs_mount_data *md, uint64_t block_no) {
    md->block_bitmap->ops->mark_free(md->block_bitmap, block_no);
}

void sfs_release_block_range(sfs_mount_data *md, block_range range) {
    block_no_t block_no = range.first_block_no;
    uint32_t count = range.blocks_count;
    while (count-- > 0) {
        md->block_bitmap->ops->mark_free(md->block_bitmap, block_no);
        block_no++;
    }
}

void sfs_release_block_range_array(sfs_mount_data *md, block_range *arr, size_t arr_items) {
    for (unsigned i = 0; i < arr_items; i++) {
        if (arr[i].first_block_no == 0 || arr[i].blocks_count == 0)
            continue;
        
        sfs_release_block_range(md, arr[i]);
    }
}

