#pragma once
#include "../filesys2/vfs_objects/block_device.h"

error_t create_ram_block_device(const char *name, size_t block_size, size_t block_count, block_device_t **result);
