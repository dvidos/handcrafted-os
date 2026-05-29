#include "sfs_internal.h"


error_t sfs_allocate_new_block(sfs_mount_data *md, uint64_t preferred_block, uint64_t *block_no) {
    *block_no = 0;

    if (preferred_block > 0) {
        if (md->block_bitmap->ops->is_free(md->block_bitmap, preferred_block)) {
            *block_no = preferred_block;
        }
    }

    // fallback, if preferred was not available
    if (*block_no == 0) {
        // interestingly, our bitmap supports block_no only up to 32 bit...
        uint32_t block_32 = 0;
        if (md->block_bitmap->ops->find_next_free(md->block_bitmap, &block_32))
            *block_no = block_32;
    }
        
    if (*block_no == 0)
        return traceable(ERR_NO_SPACE_LEFT);

    // mark as used, and clear the block for security
    md->block_bitmap->ops->mark_used(md->block_bitmap, *block_no);
    md->block_cache->ops->fill(md->block_cache, *block_no, 0);

    return OK;
}

