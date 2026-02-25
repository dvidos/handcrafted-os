#pragma once
#include "../../utils/mutex.h"
#include "../../devices/block/block_device.h"


typedef struct superblock superblock_t;
typedef struct fs_driver_ops fs_driver_ops_t;


// lives for duration of mount()
struct superblock {       
    fs_driver_ops_t *driver;      // plugin contract
    block_device_t *dev;          // partition / disk
    void *driver_priv_data;       // FS-specific superblock data
    int fs_id;                    // unique mount id (= global_monotonic_counter++)
    lock_t lock;                  // protects fs-level metadata
};


struct superblock_ops {
    superblock_t *(*create)(fs_driver_ops_t *driver, block_device_t *dev);
    void (*destroy)(superblock_t *sb);
    // log_debug?
};

extern struct superblock_ops superblocks;
