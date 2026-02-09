#pragma once
#include "../filesys2/vfs_objects/block_device.h"

void init_ata_block_devices();
int get_ata_block_device_count();

error_t create_ata_block_device(int index, block_device_t **result);
