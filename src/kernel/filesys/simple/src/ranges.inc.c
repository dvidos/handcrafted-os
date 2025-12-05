#include "internal.h"



static int v2_range_array_resolve_index(block_range *arr, int items, uint32_t *block_index, uint32_t *block_no) {
    block_range *range;

    for (int i = 0; i < items; i++) {
        range = &arr[i];

        // if used ranges exhausted, no point
        if (range->first_block_no == 0)
            break;

        // if index is within... range, use it
        if (*block_index < range->blocks_count) {
            *block_no = range->first_block_no + *block_index;
            return OK;
        }

        // deplete and try next range
        *block_index -= range->blocks_count;
    }

    return ERR_NOT_FOUND;
}

static int v2_range_array_last_block_no(block_range *arr, int items, uint32_t *last_block_no) {
    for (int i = items - 1; i >= 0; i--) {
        if (arr[i].first_block_no != 0) {
            *last_block_no = arr[i].first_block_no + arr[i].blocks_count - 1;
            return OK;
        }
    }

    return ERR_NOT_FOUND;
}

static int v2_range_array_expand(block_range *arr, int items, block_bitmap *bitmap, uint32_t *new_block_no, int *overflown) {
    int err;

    // first, find last used range in array
    int last_used = -1;
    for (int i = items - 1; i >= 0; i--) {
        if (arr[i].first_block_no != 0) {
            last_used = i;
            break;
        }
    }

    // if no used ranges were found, we can start with the first one
    if (last_used == -1) {
        err = bitmap_find_free_block(bitmap, new_block_no);
        if (err != OK) return err;

        bitmap_mark_block_used(bitmap, *new_block_no);
        arr[0].first_block_no = *new_block_no;
        arr[0].blocks_count = 1;
        *overflown = 0; // we don't need to overflow
        return OK;
    }

    // we found a range, we if we can expand
    *new_block_no = arr[last_used].first_block_no + arr[last_used].blocks_count;
    if (bitmap_is_block_free(bitmap, *new_block_no)) {
        bitmap_mark_block_used(bitmap, *new_block_no);
        arr[last_used].blocks_count += 1;
        *overflown = 0;
        return OK;
    }

    // we cannot expand, let's try to allocate a new range, if one exists
    if (last_used < items - 1) {
        err = bitmap_find_free_block(bitmap, new_block_no);
        if (err != OK) return err;

        bitmap_mark_block_used(bitmap, *new_block_no);
        arr[last_used + 1].first_block_no = *new_block_no;
        arr[last_used + 1].blocks_count = 1;
        *overflown = 0; // we don't need to overflow
        return OK;
    }

    // so, there are ranges, we could not expand, no allocatable ones, seems like we need to overflow.
    *new_block_no = 0;
    *overflown = 1;
    return OK;
}

static void v2_range_array_release_blocks(block_range *arr, int items, block_bitmap *bitmap) {
    for (int r = items - 1; r >= 0; r--) {
        block_range *range = &arr[r];
        if (range->first_block_no == 0)
            break;

        for (int i = range->blocks_count - 1; i >= 0; i--)
            bitmap_mark_block_free(bitmap, range->first_block_no + i);

        range->first_block_no = 0;
        range->blocks_count = 0;
    }
}

// -----------------------------------------------------------------------

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
    int err = bitmap_find_free_block(mt->bitmap, &new_block_no);
    if (err != OK) return err;

    bitmap_mark_block_used(mt->bitmap, new_block_no);
    err = bcache_wipe(mt->cache, new_block_no);
    if (err != OK) return err;

    range->first_block_no = new_block_no;
    range->blocks_count = 1;
    *block_no = new_block_no;
    return OK;
}

static inline int extend_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no) {
    uint32_t next_block_no = range_last_block_no(range) + 1;
    if (next_block_no >= mt->superblock->blocks_in_device)
        return ERR_RESOURCES_EXHAUSTED;

    if (!bitmap_is_block_free(mt->bitmap, next_block_no))
        return ERR_NOT_SUPPORTED;

    // so that block is free, we can use it.
    bitmap_mark_block_used(mt->bitmap, next_block_no);
    int err = bcache_wipe(mt->cache, next_block_no);
    if (err != OK) return err;

    range->blocks_count++;
    *block_no = next_block_no;
    return OK;
}

static inline void range_array_release_blocks_old(mounted_data *mt, block_range *ranges_array, int ranges_count) {
    for (int r = ranges_count - 1; r >= 0; r--) {
        block_range *range = &ranges_array[r];

        for (int i = range->blocks_count - 1; i >= 0; i--)
            bitmap_mark_block_free(mt->bitmap, range->first_block_no + i);

        range->first_block_no = 0;
        range->blocks_count = 0;
    }
}
