#include "sfs_internal.h"
#include "../../../devices/block/block_device.h"
#include "../../../logger/logger.h"
#include "../../../memory/kheap.h"

MODULE("SFS_BRW", LOG_LEVEL_DEBUG);




error_t sfs_block_read(sfs_mount_data *fs_data, uint64_t block_no, char *buffer) {
    log_trace("sfs_block_read(block_no=%llu, buffer=%p)", block_no, buffer);
    uint32_t sectors = fs_data->superblock->sectors_per_block;
    uint64_t lba = block_no * sectors;
    
    return fs_data->dev->ops->read(fs_data->dev, lba, sectors, buffer);
}

error_t sfs_block_write(sfs_mount_data *fs_data, uint64_t block_no, char *buffer) {
    log_trace("sfs_block_write(block_no=%llu, buffer=%p)", block_no, buffer);
    uint32_t sectors = fs_data->superblock->sectors_per_block;
    uint64_t lba = block_no * sectors;

    return fs_data->dev->ops->write(fs_data->dev, lba, sectors, buffer);
}

