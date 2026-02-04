#pragma once
#include <ctypes.h>
#include "../../../misc/lock.h"


typedef struct block_device block_device_t;

struct block_device {
    struct block_device_ops *ops;

    uint64_t total_blocks;
    uint32_t block_size;    // usually 512
    uint32_t flags;         // READONLY, REMOVABLE, etc
    uint32_t dev_id;        // major/minor etc.
    void *driver_data;      // ATA, NVMe, ram disk, etc
    lock_t  lock;           // to serialize access if needed
};

struct block_device_ops {
    int (*read)(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer);
    int (*write)(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);
    int (*flush)(block_device_t *dev);
};
