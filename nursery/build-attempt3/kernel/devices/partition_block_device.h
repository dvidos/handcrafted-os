#pragma once
#include <filesys2/vfs_objects/block_device.h>

error_t create_partition_block_device(
    block_device_t *underlying,
    uint64_t first_block,
    size_t num_blocks,
    block_device_t **result
);
