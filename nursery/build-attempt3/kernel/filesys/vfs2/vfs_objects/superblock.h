#pragma once
#include "../../../misc/lock.h"
#include "block_device.h"


typedef struct superblock superblock_t;
typedef struct fs_driver_ops fs_driver_ops_t;


// lives for duration of mount()
struct superblock {       
    fs_driver_ops_t *driver;      // plugin contract
    block_device_t *dev;          // partition / disk
    void *fs_private_data;        // FS-specific superblock data
    int fs_id;                    // unique mount id (= global_monotonic_counter++)
    lock_t lock;                  // protects fs-level metadata
};
