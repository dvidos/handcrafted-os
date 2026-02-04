#pragma once
#include "superblock.h"
#include "../memory/kheap.h"
#include "../klib/string.h"


static uint32_t monotonic_fs_id_counter = 0;

static superblock_t *_superblock_create(fs_driver_ops_t *driver, struct block_device *dev) {
    superblock_t *sb = (superblock_t *)kmalloc(sizeof(superblock_t));

    sb->driver = driver;            // plugin contract
    sb->dev = dev;                // partition / disk
    sb->fs_private_data = 0;        // FS-specific superblock data
    sb->fs_id = ++monotonic_fs_id_counter;    // unique mount id (= global_monotonic_counter++)
    sb->lock = 0;                   // protects fs-level metadata

    return sb;
}

static void _superblock_destroy(superblock_t *sb) {
    if (sb == NULL)
        return;
    kfree(sb);
}

struct superblock_ops superblocks = {
    .create  = _superblock_create,
    .destroy = _superblock_destroy,
};
