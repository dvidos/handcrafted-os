#include "internal.h"


static inline int is_range_used(block_range *range) {
    return range->first_block_no != 0;
}

static inline int is_range_empty(block_range *range) {
    return range->first_block_no == 0;
}

static inline uint32_t range_last_block_no(block_range *range) {
    return range->first_block_no + range->blocks_count - 1;
}

static inline int check_or_consume_blocks_in_range(block_range *range, uint32_t *relative_block_no, uint32_t *absolute_block_no) {
    if (*relative_block_no < range->blocks_count) {
        // we are within range!
        *absolute_block_no = range->first_block_no + *relative_block_no;
        return 1;
    } else {
        // we are not within range, consume this one
        *relative_block_no -= range->blocks_count;
        return 0;
    }
}

static inline void find_last_used_and_first_free_range(block_range *ranges_array, int ranges_count, int *last_used_idx, int *first_free_idx) {
    if (is_range_empty(&ranges_array[0])) {
        *last_used_idx = -1;
        *first_free_idx = 0;
    } else if (is_range_used(&ranges_array[ranges_count - 1])) {
        *last_used_idx = ranges_count - 1;
        *first_free_idx = -1;
    } else {
        for (int i = 1; i < ranges_count; i++) {
            if (is_range_used(&ranges_array[i - 1]) && is_range_empty(&ranges_array[i])) {
                *last_used_idx = i - 1;
                *first_free_idx = i;
                return;
            }
        }

        // should not happen, but...
        *last_used_idx = -1;
        *first_free_idx = -1;
    }
}

static inline int initialize_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no) {
    uint32_t new_block_no = 0;
    int err = mt->bitmap->find_a_free_block(mt->bitmap, &new_block_no);
    if (err != OK) return err;

    mt->bitmap->mark_block_used(mt->bitmap, new_block_no);
    mt->cache->wipe(mt->cache, new_block_no);
    range->first_block_no = new_block_no;
    range->blocks_count = 1;
    *block_no = new_block_no;
    return OK;
}

static inline int extend_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no) {
    uint32_t next_block_no = range_last_block_no(range) + 1;
    if (!mt->bitmap->is_block_free(mt->bitmap, next_block_no))
        return ERR_NOT_SUPPORTED;

    // so that block is free, we can use it.
    mt->bitmap->mark_block_used(mt->bitmap, next_block_no);
    mt->cache->wipe(mt->cache, next_block_no);
    range->blocks_count++;
    *block_no = next_block_no;
    return OK;
}

