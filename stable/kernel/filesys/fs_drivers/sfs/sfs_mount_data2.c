#include "sfs_internal.h"
#include "../../../devices/block/block_device.h"
#include "../../../memory/kheap.h"
#include "../../../klib/string.h"










error_t sfs_create_fs_mount_data(block_device_t *dev, sfs_mount_data **out) {
    error_t err;
    
    sfs_mount_data *md = (sfs_mount_data *)kmalloc(sizeof(sfs_mount_data));
    if (md == NULL) goto no_mem;
    memset(md, 0, sizeof(sfs_mount_data));

    // this to be used for reading superblock, as well as indirect blocks
    md->generic_block_buffer = kmalloc(MAX_BLOCK_SIZE_BYTES);
    if (md->generic_block_buffer == NULL) goto no_mem;

    // allocate and load superblock (via device, cache uses superblock)
    if (dev->block_size != sizeof(stored_superblock)) {
        log_error("Superblock size is %d, while device supports blocks of %d", sizeof(stored_superblock), dev->block_size);
        err = ERR_INVALID_ARGS;
        goto error;
    }
    md->superblock = kmalloc(sizeof(stored_superblock));
    if (md->superblock == NULL) goto no_mem;
    err = dev->ops->read_sectors(dev, 0, 1, md->generic_block_buffer);
    if (err) goto error;
    memcpy(md->superblock, md->generic_block_buffer, sizeof(stored_superblock));

    log_debug("superblock loaded, magic=%c%c%c%c, block_size=%lu, blocks=%lu, bitmap=[%lu, %lu]",
        md->superblock->magic[0], md->superblock->magic[1], md->superblock->magic[2], md->superblock->magic[3], 
        md->superblock->block_size_in_bytes,
        md->superblock->blocks_in_device,
        md->superblock->blocks_bitmap_first_block, md->superblock->blocks_bitmap_num_blocks
    );

    // setup block cache, to allow loading of data
    md->dev = dev;
    md->is_readonly = false;
    md->block_cache = create_backed_cache(md->superblock->block_size_in_bytes, BLOCKS_CACHE_CAPACITY, (backed_cache_backend){
        .load = sfs_block_cache_backend_load,
        .write = sfs_block_cache_backend_write,
        .context = md
    });
    
    // bytes we'd need to allocate for blocks bitmap
    // round up to whole blocks for easier load/save
    int bitmap_bytes = (md->superblock->blocks_in_device + 7) / 8;
    bitmap_bytes = ROUND_UP(bitmap_bytes, md->superblock->block_size_in_bytes);
    md->block_bitmap = create_bitmap(md->superblock->blocks_in_device, bitmap_bytes);
    if (md->block_bitmap == NULL) goto no_mem;

    // load bitmap from cache
    uint64_t block_no = md->superblock->blocks_bitmap_first_block;
    char *target = (char *)md->block_bitmap->words;
    for (uint32_t n = 0; n < md->superblock->blocks_bitmap_num_blocks; n++) {
        err = md->block_cache->ops->read(md->block_cache, block_no, target);
        if (err) goto error;
        block_no++;
        target += md->superblock->block_size_in_bytes;
    }
    
    // allocate inodes bitmap
    // inodes in device = number of blocks for inodes * inodes per block
    uint32_t inodes_in_device = md->superblock->inodes_array_num_blocks * md->superblock->inodes_per_block;
    int inode_bitmap_bytes = (inodes_in_device + 7) / 8;
    inode_bitmap_bytes = ROUND_UP(inode_bitmap_bytes, md->superblock->block_size_in_bytes);
    md->inode_bitmap = create_bitmap(inodes_in_device, inode_bitmap_bytes);
    if (md->inode_bitmap == NULL) goto no_mem;

    // load inode bitmap from cache
    block_no = md->superblock->inodes_bitmap_first_block;
    target = (char *)md->inode_bitmap->words;
    for (uint32_t n = 0; n < md->superblock->inodes_bitmap_num_blocks; n++) {
        err = md->block_cache->ops->read(md->block_cache, block_no, target);
        if (err) goto error;
        block_no++;
        target += md->superblock->block_size_in_bytes;
    }
    log_debug("inodes bitmap loaded.");

    // allocate inodes cache
    md->inode_cache = create_backed_cache(sizeof(stored_inode), INODES_CACHE_CAPACITY, (backed_cache_backend){
        .load = sfs_inode_cache_backend_load,
        .write = sfs_inode_cache_backend_write,
        .context = md
    });

    // Mark inode 0 (invalid) and inode 1 (root) as locked to prevent eviction
    // and signify their special status.
    md->inode_cache->ops->lock(md->inode_cache, 0); // Inode 0 is invalid
    md->inode_cache->ops->lock(md->inode_cache, 1); // Inode 1 is root dir

    *out = md;
    return OK;

no_mem:
    err = ERR_NO_MEMORY;
error:
    sfs_destroy_fs_mount_data(md);
    return traceable(err);
}

error_t sfs_sync_fs_mount_data(sfs_mount_data *md) {
    error_t err;

    // flush inodes, to update the two in-memory ones, before saving superblock
    md->inode_cache->ops->flush_all(md->inode_cache);

    // save superblock in cache
    err = md->block_cache->ops->write_part(md->block_cache, 0, 0, md->superblock, sizeof(stored_superblock));
    if (err) return traceable(err);
    
    // save bitmap in cache
    uint64_t block = md->superblock->blocks_bitmap_first_block;
    char *target = (char *)md->block_bitmap->words;
    for (uint32_t n = 0; n < md->superblock->blocks_bitmap_num_blocks; n++) {
        err = md->block_cache->ops->write(md->block_cache, block, target);
        if (err) return traceable(err);
        block++;
        target += md->superblock->block_size_in_bytes;
    }

    // finally, flush blocks cache
    md->block_cache->ops->flush_all(md->block_cache);

    // save inode bitmap to disk
    block = md->superblock->inodes_bitmap_first_block;
    target = (char *)md->inode_bitmap->words;
    for (uint32_t n = 0; n < md->superblock->inodes_bitmap_num_blocks; n++) {
        err = md->block_cache->ops->write(md->block_cache, block, target);
        if (err) return traceable(err);
        block++;
        target += md->superblock->block_size_in_bytes;
    }
    
    // finally, flush blocks cache again, to make sure inode bitmap is written
    md->block_cache->ops->flush_all(md->block_cache);
    return OK;
}

void sfs_destroy_fs_mount_data(sfs_mount_data *md) {
    if (md->inode_cache) {
        if (md->inode_cache->ops->is_locked(md->inode_cache, 0)) // Inode 0 (invalid)
            md->inode_cache->ops->unlock(md->inode_cache, 0);
        if (md->inode_cache->ops->is_locked(md->inode_cache, 1)) // Inode 1 (root dir)
            md->inode_cache->ops->unlock(md->inode_cache, 1);
    }

    if (md->superblock)           kfree(md->superblock);
    if (md->generic_block_buffer) kfree(md->generic_block_buffer);
    if (md->block_bitmap)         md->block_bitmap->ops->destroy(md->block_bitmap);
    if (md->inode_bitmap)         md->inode_bitmap->ops->destroy(md->inode_bitmap); // Destroy inode bitmap
    if (md->block_cache)          md->block_cache->ops->destroy(md->block_cache);
    if (md->inode_cache)          md->inode_cache->ops->destroy(md->inode_cache);
    kfree(md);
}
