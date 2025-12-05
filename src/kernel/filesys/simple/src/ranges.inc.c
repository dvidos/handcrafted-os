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
            continue;

        for (int i = range->blocks_count - 1; i >= 0; i--)
            bitmap_mark_block_free(bitmap, range->first_block_no + i);

        range->first_block_no = 0;
        range->blocks_count = 0;
    }
}

// -----------------------------------------------------------------------

static inline int is_range_empty(const block_range *range) {
    return range->first_block_no == 0;
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

