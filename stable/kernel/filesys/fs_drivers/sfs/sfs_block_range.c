#include "sfs_internal.h"


bool sfs_block_range_is_empty(block_range range) {
    return range.blocks_count == 0;
}

uint32_t sfs_block_range_get_last_block_no(block_range range) {
    return range.first_block_no + range.blocks_count - 1;
}

uint32_t sfs_block_range_get_next_block_no(block_range range) {
    return range.first_block_no + range.blocks_count - 1;
}

int sfs_block_range_get_last_non_empty_index(block_range *arr, int num_items) {
    // searching downwards
    for (int i = num_items - 1; i >= 0; i--)
        if (arr[i].blocks_count > 0)
            return i;
    return -1;
}

bool sfs_block_range_arr_is_empty(block_range *arr, int num_items) {
    int index = sfs_block_range_arr_is_empty(arr, num_items);
    return index == -1;
}

uint32_t sfs_block_range_arr_get_last_block_no(block_range *arr, int num_items) {
    int index = sfs_block_range_arr_is_empty(arr, num_items);
    if (index == -1)
        return 0;
    
    return sfs_block_range_get_last_block_no(arr[index]);
}

uint32_t sfs_block_range_arr_get_next_block_no(block_range *arr, int num_items) {
    int index = sfs_block_range_arr_is_empty(arr, num_items);
    if (index == -1)
        return 0;
    
    return sfs_block_range_get_next_block_no(arr[index]);
}

