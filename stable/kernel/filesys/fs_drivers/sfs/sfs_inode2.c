#include "sfs_internal.h"


error_t sfs_inode2_read_from_device(sfs_mount_data *mt, inode_no_t num, stored_inode *inode) {
    log_trace("sfs_inode2_read_from_device(inode_no=%lu)", num);
    ASSERT(mt->superblock->inodes_array_first_block > 0);
    ASSERT(mt->superblock->inodes_per_block > 0);
    ASSERT(mt->generic_block_buffer != NULL);

    // DEBT: improve this to read only one sector from device, not a whole block
    uint32_t block_num = mt->superblock->inodes_array_first_block + (num / mt->superblock->inodes_per_block);
    uint32_t sectors = mt->superblock->sectors_per_block;
    uint32_t lba = block_num * sectors;
    
    error_t err = mt->dev->ops->read_sectors(mt->dev, lba, sectors, mt->generic_block_buffer);
    if (err) return traceable(err);

    uint32_t offset = (num % mt->superblock->inodes_per_block) * sizeof(stored_inode);
    memcpy(inode, mt->generic_block_buffer + offset, sizeof(stored_inode));

    return OK;
}

error_t sfs_inode2_write_to_device(sfs_mount_data *mt, inode_no_t num, stored_inode *inode) {
    log_trace("sfs_inode2_write_to_device(inode_no=%lu)", num);
    ASSERT(mt->superblock->inodes_array_first_block > 0);
    ASSERT(mt->superblock->inodes_per_block > 0);
    ASSERT(mt->generic_block_buffer != NULL);
    error_t err;

    // DEBT: improve this to read only one sector from device, not a whole block
    uint32_t block_num = mt->superblock->inodes_array_first_block + (num / mt->superblock->inodes_per_block);
    uint32_t sectors = mt->superblock->sectors_per_block;
    uint32_t lba = block_num * sectors;
    
    err = mt->dev->ops->read_sectors(mt->dev, lba, sectors, mt->generic_block_buffer);
    if (err) return traceable(err);

    uint32_t offset = (num % mt->superblock->inodes_per_block) * sizeof(stored_inode);
    memcpy(mt->generic_block_buffer + offset, inode, sizeof(stored_inode));

    err = mt->dev->ops->write_sectors(mt->dev, lba, sectors, mt->generic_block_buffer);
    if (err) return traceable(err);

    return OK;
}

error_t sfs_inode_cache_backend_load(uint64_t key, void *obj_data, void *context) {
    log_trace("sfs_inode_cache_backend_load(key=%llu, data=%p, ctx=%p)", key, obj_data, context);
    sfs_mount_data *md = (sfs_mount_data *)context;
    // The key is the inode_no_t
    return sfs_inode2_read_from_device(md, (inode_no_t)key, (stored_inode *)obj_data);
}

error_t sfs_inode_cache_backend_write(uint64_t key, void *obj_data, void *context) {
    log_trace("sfs_inode_cache_backend_write(key=%llu, data=%p, ctx=%p)", key, obj_data, context);
    sfs_mount_data *md = (sfs_mount_data *)context;
    // The key is the inode_no_t
    return sfs_inode2_write_to_device(md, (inode_no_t)key, (stored_inode *)obj_data);
}

error_t sfs_load_inode2(sfs_mount_data *mt, inode_no_t num, stored_inode *inode) {
    return mt->inode_cache->ops->read(mt->inode_cache, num, inode);
}

error_t sfs_save_inode2(sfs_mount_data *mt, inode_no_t num, stored_inode *inode) {
    return mt->inode_cache->ops->write(mt->inode_cache, num, inode);
}

error_t sfs_allocate_inode2(sfs_mount_data *mt, inode_no_t *new_inode_num) {
    uint32_t num = 0;
    bool exists = mt->inode_bitmap->ops->find_next_free(mt->inode_bitmap, &num);
    if (!exists) return ERR_NO_SPACE_LEFT;

    mt->inode_bitmap->ops->mark_used(mt->inode_bitmap, num);
    mt->inode_cache->ops->fill(mt->inode_cache, num, 0);
    *new_inode_num = num;
    return OK;
}

error_t sfs_release_inode2(sfs_mount_data *mt, inode_no_t num) {
    if (mt->inode_bitmap->ops->is_free(mt->inode_bitmap, num)) {
        log_error("Releasing inode %lu, which is not allocated", num);
        return OK;
    }

    mt->inode_cache->ops->fill(mt->inode_cache, num, 0);
    mt->inode_bitmap->ops->mark_free(mt->inode_bitmap, num);
    return OK;
}
