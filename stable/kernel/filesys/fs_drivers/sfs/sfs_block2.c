#include "sfs_internal.h"



error_t sfs_block_read_from_device(sfs_mount_data *ms, uint64_t block_no, void *buffer) {
    log_trace("sfs_block_read_from_device(block_no=%llu, buffer=%p)", block_no, buffer);
    uint32_t sectors = ms->superblock->sectors_per_block;
    uint64_t lba = block_no * sectors;
    
    return ms->dev->ops->read_sectors(ms->dev, lba, sectors, buffer);
}

error_t sfs_block_write_to_device(sfs_mount_data *md, uint64_t block_no, const void *buffer) {
    log_trace("sfs_block_write_to_device(block_no=%llu, buffer=%p)", block_no, buffer);
    uint32_t sectors = md->superblock->sectors_per_block;
    uint64_t lba = block_no * sectors;

    return md->dev->ops->write_sectors(md->dev, lba, sectors, buffer);
}

error_t sfs_block_cache_backend_load(uint64_t key, void *obj_data, void *context) {
    log_trace("block_cache_load_block(key=%llu, obj_data=%p)", key, obj_data);
    return sfs_block_read_from_device((sfs_mount_data *)context, key, obj_data);
}

error_t sfs_block_cache_backend_write(uint64_t key, void *obj_data, void *context) {
    log_trace("block_cache_write_block(key=%llu, obj_data=%p)", key, obj_data);
    return sfs_block_write_to_device((sfs_mount_data *)context, key, obj_data);
}

error_t sfs_cached_read(sfs_mount_data *md, uint64_t block_no, size_t block_offset, void *buffer, size_t buffer_size) {
    return md->block_cache->ops->read_part(md->block_cache, block_no, block_offset, buffer, buffer_size);
}

error_t sfs_cached_write(sfs_mount_data *md, uint64_t block_no, size_t block_offset, const void *buffer, size_t buffer_size) {
    return md->block_cache->ops->write_part(md->block_cache, block_no, block_offset, buffer, buffer_size);
}

error_t sfs_cached_fill(sfs_mount_data *md, uint64_t block_no, char value) {
    return md->block_cache->ops->fill(md->block_cache, block_no, value);
}

