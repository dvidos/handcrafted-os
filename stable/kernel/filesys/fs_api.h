#pragma once
#include "../devices/block/block_device.h"
#include "fs_driver.h"
#include "dev_driver.h"



// initial probing of storage devices is outside of VFS
void fs_register(fs_driver_t *drv);

fs_driver_t *fs_probe(block_device_t *dev);
error_t fs_mkfs(block_device_t *dev, fs_driver_t *drv);



void fs_register_device(const char *name, dev_driver_t *drv, int dev_number);
const device_t *fs_lookup_device(const char *name);
