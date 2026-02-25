#pragma once
#include "block_device.h"

error_t create_ram_block_device(
    const char *id,
    const char *name,
    size_t block_size,
    size_t block_count,
    char *data,
    block_device_t **result
);


