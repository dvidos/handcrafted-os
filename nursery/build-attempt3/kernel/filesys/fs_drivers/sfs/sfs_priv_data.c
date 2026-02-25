#include "sfs_internal.h"
#include "../../../devices/block/block_device.h"
#include "../../../memory/kheap.h"
#include "../../../klib/string.h"
#include "../../../utils/logger.h"

MODULE("SFS_PRIV", LOG_LEVEL_DEBUG);


// cache backend functions
static error_t block_cache_load_block(uint64_t key, void *obj_data, void *context);
static error_t block_cache_write_block(uint64_t key, void *obj_data, void *context);
static error_t inode_cache_load_inode(uint64_t key, void *obj_data, void *context);
static error_t inode_cache_write_inode(uint64_t key, void *obj_data, void *context);


// ----------------------------------------------------------------------------------

error_t sfs_create_fs_data(block_device_t *dev, sfs_mount_data **out) {
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
    err = dev->ops->read(dev, 0, 1, md->generic_block_buffer);
    if (err) goto error;
    memcpy(md->superblock, md->generic_block_buffer, sizeof(stored_superblock));

    log_debug("superblock loaded, magic=%c%c%c%c, block_size=%lu, blocks=%lu, bitmap=[%lu, %lu], inodes_db_recs=%lu",
        md->superblock->magic[0], md->superblock->magic[1], md->superblock->magic[2], md->superblock->magic[3], 
        md->superblock->block_size_in_bytes,
        md->superblock->blocks_in_device,
        md->superblock->blocks_bitmap_first_block, md->superblock->blocks_bitmap_blocks_count,
        md->superblock->inodes_db_rec_count
    );

    // setup block cache, to allow loading of data
    md->dev = dev;
    md->is_readonly = false;
    md->block_cache = create_backed_cache(md->superblock->block_size_in_bytes, BLOCKS_CACHE_CAPACITY, (backed_cache_backend){
        .load = block_cache_load_block,
        .write = block_cache_write_block,
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
    for (uint32_t n = 0; n < md->superblock->blocks_bitmap_blocks_count; n++) {
        err = md->block_cache->ops->read(md->block_cache, block_no, target);
        if (err) goto error;
        block_no++;
        target += md->superblock->block_size_in_bytes;
    }
    
    char first_bits_string[128] = {0};
    for (uint32_t i = 0; i < 32; i++) first_bits_string[i] = md->block_bitmap->ops->is_free(md->block_bitmap, i) ? 'f' : 'u';
    first_bits_string[32] = 0;
    log_debug("blocks bitmap loaded, first bits (free/used): %s...", first_bits_string);

    // allocate inodes cache
    md->inode_cache = create_backed_cache(sizeof(stored_inode), INODES_CACHE_CAPACITY, (backed_cache_backend){
        .load = inode_cache_load_inode,
        .write = inode_cache_write_inode,
        .context = md
    });

    // load and mark as non-evictable
    md->inode_cache->ops->lock(md->inode_cache, INODE_DB_INODE_ID);
    md->inode_cache->ops->lock(md->inode_cache, ROOT_DIR_INODE_ID);

    *out = md;
    return OK;

no_mem:
    err = ERR_NO_MEMORY;
error:
    sfs_destroy_fs_data(md);
    return err;
}

error_t sfs_sync_fs_data(sfs_mount_data *md) {
    error_t err;

    // flush inodes, to update the two in-memory ones, before saving superblock
    md->inode_cache->ops->flush_all(md->inode_cache);

    // save superblock in cache
    err = md->block_cache->ops->write_part(md->block_cache, 0, 0, md->superblock, sizeof(stored_superblock));
    if (err) return err;
    
    // save bitmap in cache
    uint64_t block = md->superblock->blocks_bitmap_first_block;
    char *target = (char *)md->block_bitmap->words;
    for (uint32_t n = 0; n < md->superblock->blocks_bitmap_blocks_count; n++) {
        err = md->block_cache->ops->write(md->block_cache, block, target);
        if (err) return err;
        block++;
        target += md->superblock->block_size_in_bytes;
    }

    // finally, flush blocks cache
    md->block_cache->ops->flush_all(md->block_cache);
    return OK;
}

void sfs_destroy_fs_data(sfs_mount_data *md) {
    if (md == NULL)
        return;

    if (md->inode_cache) {
        if (md->inode_cache->ops->is_locked(md->inode_cache, INODE_DB_INODE_ID))
            md->inode_cache->ops->unlock(md->inode_cache, INODE_DB_INODE_ID);
        if (md->inode_cache->ops->is_locked(md->inode_cache, ROOT_DIR_INODE_ID))
            md->inode_cache->ops->unlock(md->inode_cache, ROOT_DIR_INODE_ID);
    }

    if (md->superblock)           kfree(md->superblock);
    if (md->generic_block_buffer) kfree(md->generic_block_buffer);
    if (md->block_bitmap)         md->block_bitmap->ops->destroy(md->block_bitmap);
    if (md->block_cache)          md->block_cache->ops->destroy(md->block_cache);
    if (md->inode_cache)          md->inode_cache->ops->destroy(md->inode_cache);
    kfree(md);
}

// -------------------------------------------------------------------

static error_t block_cache_load_block(uint64_t key, void *obj_data, void *context) {
    log_trace("block_cache_load_block(key=%llu, obj_data=%p)", key, obj_data);
    return sfs_block_read((sfs_mount_data *)context, key, obj_data);
}

static error_t block_cache_write_block(uint64_t key, void *obj_data, void *context) {
    log_trace("block_cache_write_block(key=%llu, obj_data=%p)", key, obj_data);
    return sfs_block_write((sfs_mount_data *)context, key, obj_data);
}

static error_t inode_cache_load_inode(uint64_t key, void *obj_data, void *context) {
    log_trace("inode_cache_load_inode(key=%llu%s, obj_data=%p)", key, ((key == INODE_DB_INODE_ID) ? " (inode_db)" : (key == ROOT_DIR_INODE_ID ? " (root_dir)" : "")), obj_data);
    sfs_mount_data *md = (sfs_mount_data *)context;

    if (key == INODE_DB_INODE_ID) {
        memcpy(obj_data, &md->superblock->inodes_db_inode, sizeof(stored_inode));
        return OK;
    } else if (key == ROOT_DIR_INODE_ID) {
        memcpy(obj_data, &md->superblock->root_dir_inode, sizeof(stored_inode));
        return OK;
    } else {
        // read record from inodes database
        ssize_t bytes = sfs_node_read_file_rec(md, &md->superblock->inodes_db_inode, sizeof(stored_inode), key, obj_data);
        if (bytes < 0) return (error_t)bytes;
        return OK;
    }

    return ERR_NOT_IMPLEMENTED;
}

static error_t inode_cache_write_inode(uint64_t key, void *obj_data, void *context) {
    log_trace("inode_cache_write_inode(key=%llu%s, obj_data=%p)", key, ((key == INODE_DB_INODE_ID) ? " (inode_db)" : (key == ROOT_DIR_INODE_ID ? " (root_dir)" : "")), obj_data);
    sfs_mount_data *md = (sfs_mount_data *)context;

    if (key == INODE_DB_INODE_ID) {
        memcpy(&md->superblock->inodes_db_inode, obj_data, sizeof(stored_inode));
        return OK;
    } else if (key == ROOT_DIR_INODE_ID) {
        memcpy(&md->superblock->root_dir_inode, obj_data, sizeof(stored_inode));
        return OK;
    } else {
        // write record in inodes database
        ssize_t bytes = sfs_node_write_file_rec(md, &md->superblock->inodes_db_inode, INODE_DB_INODE_ID, sizeof(stored_inode), key, obj_data);
        if (bytes < 0) return (error_t)bytes;
        return OK;
    }

    return ERR_NOT_IMPLEMENTED;
}
