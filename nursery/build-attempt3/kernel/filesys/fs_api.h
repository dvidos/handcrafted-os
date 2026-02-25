#pragma once
#include "../devices/block/block_device.h"
#include "fs_drivers/fs_driver.h"



// initial probing of storage devices is outside of VFS
void fs_register(fs_driver_t *drv);

fs_driver_t *fs_probe(block_device_t *dev);
error_t fs_mkfs(block_device_t *dev, fs_driver_t *drv);
