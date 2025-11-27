#include "internal.h"


static inline int is_range_used(const block_range *range) {
    return range->first_block_no != 0;
}

static inline int is_range_empty(const block_range *range) {
    return range->first_block_no == 0;
}

static inline uint32_t range_last_block_no(const block_range *range) {
    return range->first_block_no + range->blocks_count - 1;
}

static inline int check_or_consume_blocks_in_range(const block_range *range, uint32_t *relative_block_no, uint32_t *absolute_block_no) {
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

static inline void find_last_used_and_first_free_range(const block_range *ranges_array, int ranges_count, int *last_used_idx, int *first_free_idx) {
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
    int err = find_next_free_block(mt, &new_block_no);
    if (err != OK) return err;

    mark_block_used(mt, new_block_no);
    cached_wipe(mt->cache, new_block_no);
    range->first_block_no = new_block_no;
    range->blocks_count = 1;
    *block_no = new_block_no;
    return OK;
}

static inline int extend_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no) {
    uint32_t next_block_no = range_last_block_no(range) + 1;
    if (!is_block_free(mt, next_block_no))
        return ERR_NOT_SUPPORTED;

    // so that block is free, we can use it.
    mark_block_used(mt, next_block_no);
    cached_wipe(mt->cache, next_block_no);
    range->blocks_count++;
    *block_no = next_block_no;
    return OK;
}

static inline void range_array_release_blocks(mounted_data *mt, block_range *ranges_array, int ranges_count) {
    for (int i = ranges_count - 1; i >= 0; i--) {
        block_range *range = &ranges_array[i];

        for (int i = range->blocks_count - 1; i >= 0; i--)
            mark_block_free(mt, range->first_block_no + i);

        range->first_block_no = 0;
        range->blocks_count = 0;
    }
}