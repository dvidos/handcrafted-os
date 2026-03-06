#include "../fs_driver.h"
#include "../../../include/uapi/errors.h"
#include "../../../memory/kheap.h"
#include "../../../drivers/clock.h"
#include "../../../klib/string.h"
#include "../../../klib/cache.h"
#include "../../../klib/list.h"
#include "../../../logger/logger.h"
#include "sfs_internal.h"


MODULE("SFS", LOG_LEVEL_WARN);

#define KB (1024)
#define MB (1024 * KB)



static bool sb_recognized(block_device_t *dev) {

    char *buffer = kmalloc(dev->block_size);
    error_t err = dev->ops->read(dev, 0, 1, buffer);
    if (err) { kfree(buffer); return false; }
    
    if (!err) {
        // log_debug("Probing device '%s', first bytes follow", dev->id);
        // log_debug_hex(buffer, 32, 0);
        // 0.036 SFS        DEBUG Cluster 0 from device 'sata0p0'
        // 0.036 SFS        DEBUG 00000000: 53 46 53 31 40 00 40 00  00 00 00 00 00 00 00 00  SFS1@.@. ........
        // 0.037 SFS        DEBUG 00000010: 00 02 00 00 02 00 00 00  00 04 00 00 33 1F 00 00  ........ ....3...
    }
    
    stored_superblock *sb = (stored_superblock *)buffer;
    bool recognized = true;
    
    if (memcmp(sb->magic, "SFS1", sizeof(sb->magic)) != 0)
        recognized = false;
    if (sb->direntry_size != sizeof(stored_dir_entry) || sb->inode_size != sizeof(stored_inode))
        recognized = false;
    if (sb->sector_size < 512 || sb->sector_size > MAX_BLOCK_SIZE_BYTES)
        recognized = false;
    if (sb->block_size_in_bytes < 512 || sb->block_size_in_bytes > MAX_BLOCK_SIZE_BYTES)
        recognized = false;

    kfree(buffer);
    return recognized;
}

// -------------------------------------------------------------------------

static error_t sfs_driver_probe(block_device_t *dev) {
    log_trace("sfs_driver_probe()");
    return sb_recognized(dev) ? OK : ERR_NOT_SUPPORTED;
}

static error_t sfs_driver_mount(superblock_t *sb) {
    log_trace("sfs_driver_mount()");
    if (!sb_recognized(sb->dev))
        return traceable(ERR_NOT_SUPPORTED);

    sfs_mount_data *md;
    error_t err = sfs_create_fs_data(sb->dev, &md);
    if (err) return err;

    sb->driver_priv_data = md;
    return OK;
}

static error_t sfs_driver_unmount(superblock_t *sb) {
    log_trace("sfs_driver_unmount()");
    sfs_mount_data *fs_data = (sfs_mount_data *)sb->driver_priv_data;

    error_t err = sfs_sync_fs_data(fs_data);
    if (err) return err;

    sfs_destroy_fs_data(fs_data);
    sb->driver_priv_data = NULL;
    return OK;
}

static error_t sfs_driver_sync(superblock_t *sb) {
    log_trace("sfs_driver_sync()");
    return sfs_sync_fs_data((sfs_mount_data *)sb->driver_priv_data);
}

static int auto_determine_block_size(uint32_t sector_size, uint64_t capacity) {
    uint32_t block_size = 0;

    if      (capacity <=  2 * MB) block_size =  512;
    else if (capacity <=  8 * MB) block_size = 1024;
    else if (capacity <= 32 * MB) block_size = 2048;
    else                          block_size = MAX_BLOCK_SIZE_BYTES;
    
    if (block_size < sector_size)
        block_size = sector_size;

    return block_size;
}

static error_t sfs_driver_mkfs(block_device_t *dev) {
    error_t err;
    stored_superblock *sb = NULL;
    bitmap_t *bitmap = NULL;

    uint64_t dev_capacity_bytes = dev->block_size * dev->total_blocks;
    uint32_t fs_block_size = auto_determine_block_size(dev->block_size, dev_capacity_bytes);
    uint32_t sectors_per_block = fs_block_size / dev->block_size;

    // --- superblock ---
    sb = kmalloc(sizeof(stored_superblock));
    memset(sb, 0, sizeof(stored_superblock));
    memcpy(sb->magic, "SFS1", sizeof(sb->magic));
    sb->direntry_size = sizeof(stored_dir_entry);
    sb->inode_size = sizeof(stored_inode);
    sb->block_size_in_bytes = fs_block_size;
    sb->sector_size = dev->block_size;
    sb->sectors_per_block = sectors_per_block;
    sb->blocks_in_device = dev->total_blocks / sectors_per_block;
    strncpy(sb->volume_label, "SFS_VOL", sizeof(sb->volume_label));

    // --- block bitmap ---
    uint32_t bitmap_size_in_bits = sb->blocks_in_device;
    uint32_t bitmap_size_in_bytes = (bitmap_size_in_bits + 7) / 8;
    uint32_t bitmap_size_in_blocks = (bitmap_size_in_bytes + fs_block_size - 1) / fs_block_size;
    sb->blocks_bitmap_first_block = 1;
    sb->blocks_bitmap_blocks_count = bitmap_size_in_blocks;

    bitmap = create_bitmap(sb->blocks_in_device, bitmap_size_in_blocks);
    bitmap->ops->mark_used(bitmap, 0); // superblock
    for (uint32_t i = 0; i < bitmap_size_in_blocks; i++) {
        bitmap->ops->mark_used(bitmap, 1 + i); // bitmap block
    }

    // --- inode db ---
    sb->inodes_db_inode = new_stored_inode_file();
    block_no_t inodes_db_block;
    if (!bitmap->ops->find_next_free(bitmap, &inodes_db_block)) return traceable(ERR_NO_SPACE_LEFT);
    bitmap->ops->mark_used(bitmap, inodes_db_block);
    sb->inodes_db_inode.ranges[0].first_block_no = inodes_db_block;
    sb->inodes_db_inode.ranges[0].blocks_count = 1;
    sb->inodes_db_inode.allocated_blocks = 1;
    sb->inodes_db_rec_count = 2; // inode 0 for inodes_db, inode 1 for root_dir

    // --- root dir ---
    sb->root_dir_inode = new_stored_inode_dir();
    block_no_t root_dir_block;
    if (!bitmap->ops->find_next_free(bitmap, &root_dir_block)) return traceable(ERR_NO_SPACE_LEFT);
    bitmap->ops->mark_used(bitmap, root_dir_block);
    sb->root_dir_inode.ranges[0].first_block_no = root_dir_block;
    sb->root_dir_inode.ranges[0].blocks_count = 1;
    sb->root_dir_inode.allocated_blocks = 1;
    sb->root_dir_inode.file_size = 2 * sizeof(stored_dir_entry);

    // --- write to disk ---
    char *buffer = kmalloc(fs_block_size);
    
    // superblock
    memcpy(buffer, sb, sizeof(stored_superblock));
    err = dev->ops->write(dev, 0, sectors_per_block, buffer);
    if (err) goto cleanup;

    // bitmap
    bitmap = create_bitmap(sb->blocks_in_device, sb->blocks_bitmap_blocks_count * sb->block_size_in_bytes);
    err = dev->ops->write(dev, sb->blocks_bitmap_first_block, sb->blocks_bitmap_blocks_count * sb->block_size_in_bytes, bitmap->words);
    if (err) goto cleanup;

    // root dir file
    memset(buffer, 0, fs_block_size);
    stored_dir_entry *entries = (stored_dir_entry *)buffer;
    strcpy(entries[0].name, ".");
    entries[0].inode_num = ROOT_DIR_INODE_ID;
    strcpy(entries[1].name, "..");
    entries[1].inode_num = ROOT_DIR_INODE_ID;
    err = dev->ops->write(dev, root_dir_block * sectors_per_block, sectors_per_block, buffer);
    if (err) goto cleanup;

cleanup:
    kfree(buffer);
    kfree(sb);
    if (bitmap) bitmap->ops->destroy(bitmap);
    return err;
}

static error_t sfs_driver_get_root_dir(superblock_t *sb, inode_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)sb->driver_priv_data;
    *out = inodes.create(sb, ROOT_DIR_INODE_ID, true, false, md->superblock->root_dir_inode.file_size);
    return OK;
}

static error_t sfs_driver_lookup(inode_t *dir, const char *name, inode_t *out) {
    log_trace("sfs_driver_lookup(dir=%llu, name='%s')", dir->inode_num, name);
    sfs_mount_data *md = (sfs_mount_data *)dir->sb->driver_priv_data;
    stored_inode *parent_inode;
    stored_inode *target_inode;
    error_t err;

    err = md->inode_cache->ops->get(md->inode_cache, dir->inode_num, (void **)&parent_inode);
    if (err) return err;
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    uint32_t rec_no;
    uint32_t target_inode_no;
    err = sfs_node_dir_find_entry(md, parent_inode, name, &rec_no, &target_inode_no);
    if (err) return err;
    
    err = md->inode_cache->ops->get(md->inode_cache, target_inode_no, (void **)&target_inode);
    if (err) return err;

    *out = inodes.create(dir->sb, target_inode_no, STORED_INODE_IS_DIR(target_inode), STORED_INODE_IS_FILE(target_inode), target_inode->file_size);
    return OK;
}

static error_t sfs_driver_open(inode_t *n, int flags, open_file_t **file_handle) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;

    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&sin);
    if (err) return err;

    // validate first
    if (!stored_inode_is_file(sin)) return traceable(ERR_NOT_A_FILE);

    *file_handle = open_files.create(n->sb, n);

    // ensure this is not evicted until file is closed
    err = md->inode_cache->ops->lock(md->inode_cache, n->inode_num);
    if (err) return err;

    return OK;
}

static error_t sfs_driver_close(open_file_t *file) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    error_t err;

    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    
    stored_inode *sin;
    err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&sin);
    if (err) return err;

    // sin->modified_at = ...
    
    err = md->inode_cache->ops->flush(md->inode_cache, file->inode.inode_num);
    if (err) return err;
    err = md->inode_cache->ops->unlock(md->inode_cache, file->inode.inode_num);
    if (err) return err;
    
    return OK;
}

static ssize_t sfs_driver_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    stored_inode *sin;
    
    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&sin);
    if (err) return err;

    ssize_t answer = sfs_node_read_file_bytes(md, sin, offset, buf, len);
    return answer;
}

static ssize_t sfs_driver_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    stored_inode *sin;
    
    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&sin);
    if (err) return err;

    ssize_t answer = sfs_node_write_file_bytes(md, sin, file->inode.inode_num, offset, buf, len);
    return answer;
}

static error_t sfs_driver_flush(open_file_t *file) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;

    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->flush(md->inode_cache, file->inode.inode_num);
    if (err) return err;
    
    return OK;
}

static error_t sfs_driver_opendir(inode_t *dir, open_file_t **dir_handle) {
    sfs_mount_data *md = (sfs_mount_data *)dir->sb->driver_priv_data;

    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, dir->inode_num, (void **)&sin);
    if (err) return err;

    // validate first
    if (!stored_inode_is_dir(sin)) return traceable(ERR_NOT_A_DIRECTORY);

    *dir_handle = open_files.create(dir->sb, dir);

    // ensure this is not evicted until dir is closed
    err = md->inode_cache->ops->lock(md->inode_cache, dir->inode_num);
    if (err) return err;

    return OK;
}

static ssize_t sfs_driver_readdir(open_file_t *dir_handle, vfs_dirent_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)dir_handle->sb->driver_priv_data;
    error_t err;
    stored_inode *dir_sin; // The inode of the directory being read.

    if (!md->inode_cache->ops->is_locked(md->inode_cache, dir_handle->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    err = md->inode_cache->ops->get(md->inode_cache, dir_handle->inode.inode_num, (void **)&dir_sin);
    if (err) return err;

    // 'dir_handle->offset' now serves as the physical byte offset within the
    // directory's raw data blocks where the driver should start searching
    // for the next valid 'stored_dir_entry'.
    // This offset is managed by the driver and aligns with the concept of
    // an opaque stream position (cookie) for readdir operations.
    uint32_t current_physical_scan_offset = dir_handle->offset;

    // Loop to find the next valid directory entry from the current scan offset.
    while (current_physical_scan_offset < dir_sin->file_size) {
        stored_dir_entry sde;
        ssize_t bytes_read_from_physical;

        // Read one 'stored_dir_entry' from the current physical scan offset.
        bytes_read_from_physical = sfs_node_read_file_bytes(
            md, dir_sin, current_physical_scan_offset, &sde, sizeof(stored_dir_entry)
        );

        if (bytes_read_from_physical <= 0) {
            // Error during read or end of directory's physical data.
            // Update 'dir_handle->offset' to mark EOF for subsequent calls.
            dir_handle->offset = dir_sin->file_size;
            return bytes_read_from_physical; // 0 for EOF, negative for error.
        }

        // Always advance the internal scan offset by the size of the physical record just read.
        // This is crucial to traverse all physical entries, including unused ones.
        current_physical_scan_offset += bytes_read_from_physical;

        if (!stored_dir_entry_is_used(&sde)) {
            // This is an unused/deleted entry. Skip it and continue searching
            // from the next physical record. 'dir_handle->offset' is NOT updated yet.
            continue;
        }

        // --- Found a valid directory entry ---

        // Populate the 'vfs_dirent_t' for the VFS layer.
        strncpy(out->d_name, sde.name, sizeof(out->d_name) - 1);
        out->d_name[sizeof(out->d_name) - 1] = '\0'; // Ensure null-termination
        out->d_ino = sde.inode_num;

        // Determine the type of the entry's inode.
        stored_inode *entry_sin;
        err = md->inode_cache->ops->get(md->inode_cache, sde.inode_num, (void **)&entry_sin);
        if (err) {
            // If inode lookup fails, it might be a deleted inode or corrupted.
            // Report as a generic file type.
            out->d_type = S_IFREG;
        } else {
            out->d_type = (entry_sin->type_perms & STORED_INODE_TYPE_MASK);
        }
        
        // Update 'dir_handle->offset' to the physical offset where the *next*
        // 'stored_dir_entry' (used or unused) would begin. This is the new
        // "stream position" for the next call to readdir.
        dir_handle->offset = current_physical_scan_offset;

        // Return the size of a populated 'vfs_dirent_t' to the VFS.
        // This signifies that one logical directory entry has been successfully read.
        return sizeof(vfs_dirent_t);
    }

    // End of directory: no more valid entries found after scanning to the end.
    // Ensure 'dir_handle->offset' is marked as EOF.
    dir_handle->offset = dir_sin->file_size;
    return 0; // Signifies end of directory.
}

static error_t sfs_driver_rewinddir(open_file_t *dir_handle) {
    sfs_mount_data *md = (sfs_mount_data *)dir_handle->sb->driver_priv_data;
    if (!md->inode_cache->ops->is_locked(md->inode_cache, dir_handle->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    dir_handle->offset = 0;
    return OK;
}

static error_t sfs_driver_closedir(open_file_t *dir_handle) {
    sfs_mount_data *md = (sfs_mount_data *)dir_handle->sb->driver_priv_data;
    error_t err;

    if (!md->inode_cache->ops->is_locked(md->inode_cache, dir_handle->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    
    err = md->inode_cache->ops->unlock(md->inode_cache, dir_handle->inode.inode_num);
    if (err) return err;
    
    return OK;
}

static error_t sfs_driver_mkdir(inode_t *parent, const char *name, inode_t *out) { 
    sfs_mount_data *md = (sfs_mount_data *)parent->sb->driver_priv_data;
    uint32_t inode_no;
    uint32_t rec_no;
    error_t err;

    if (is_name_reserved(name))
        return traceable(ERR_INVALID_ARGS);
    
    // find our inode entry
    stored_inode *parent_sin;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_sin);
    if (err) return err;
    if (!stored_inode_is_dir(parent_sin)) return traceable(ERR_NOT_A_DIRECTORY);

    // see if already exists
    err = sfs_node_dir_find_entry(md, parent_sin, name, &rec_no, &inode_no);
    if (err == OK) return traceable(ERR_ALREADY_EXISTS);

    // create inode on disk (cache cannot do this)
    stored_inode sin = new_stored_inode_dir();
    err = sfs_inodes_db_append(md, &sin, &inode_no);
    if (err) return err;

    // add the entry in the parent directory
    err = sfs_node_dir_add_entry(md, parent->inode_num, parent_sin, name, inode_no);
    if (err) return err;

    // load our new directory from disk
    stored_inode *new_sin;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&new_sin);
    if (err) return err;

    // we now need to add the two references, to self and parent.
    err = sfs_node_dir_add_entry(md, inode_no, new_sin, ".", inode_no);
    if (err) return err;
    err = sfs_node_dir_add_entry(md, inode_no, new_sin, "..", parent->inode_num);
    if (err) return err;

    *out = inodes.create(parent->sb, inode_no, parent, name, parent->size);
    return OK;
}

static error_t sfs_driver_rmdir(inode_t *parent, const char *name) {
    sfs_mount_data *md = (sfs_mount_data *)parent->sb->driver_priv_data;
    uint32_t inode_no;
    uint32_t rec_no;
    error_t err;

    if (is_name_reserved(name))
        return traceable(ERR_INVALID_ARGS);
    
    // find our container inode
    stored_inode *parent_sin;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_sin);
    if (err) return err;
    if (!stored_inode_is_dir(parent_sin)) return traceable(ERR_NOT_A_DIRECTORY);

    // find the target entry
    err = sfs_node_dir_find_entry(md, parent_sin, name, &rec_no, &inode_no);
    if (err) return err;

    // load the target dir, to verify if empty
    stored_inode *dying_sin;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&dying_sin);
    if (err) return err;
    if (!stored_inode_is_dir(dying_sin)) return traceable(ERR_NOT_A_DIRECTORY);
    
    // see if it is not empty, ignoring special entries
    bool is_empty = false;
    err = sfs_node_dir_is_empty(md, dying_sin, true, &is_empty);
    if (err) return err;
    if (!is_empty) return traceable(ERR_DIR_NOT_EMPTY);

    // first, remove entry from parent directory
    err = sfs_node_dir_set_entry(md, parent->inode_num, parent_sin, rec_no, "", INVALID_INODE_NO);
    if (err) return err;

    // then, release directory's own data blocks
    err = sfs_node_release_all_data_blocks(md, inode_no);
    if (err) return err;

    // finally, invalidate the inode
    dying_sin->type_perms = 0;
    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_no);

    return OK;
}

static error_t sfs_driver_create(inode_t *parent, const char *name, int type, inode_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)parent->sb->driver_priv_data;
    uint32_t inode_no;
    uint32_t rec_no;
    error_t err;

    if (is_name_reserved(name))
        return traceable(ERR_INVALID_ARGS);
    
    // find our inode entry
    stored_inode *parent_sin;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_sin);
    if (err) return err;
    if (!stored_inode_is_dir(parent_sin)) return traceable(ERR_NOT_A_DIRECTORY);

    // see if already exists
    err = sfs_node_dir_find_entry(md, parent_sin, name, &rec_no, &inode_no);
    if (err == OK) return traceable(ERR_ALREADY_EXISTS);

    // create inode on disk (cache cannot do this)
    stored_inode sin = new_stored_inode_file();
    sin.created_at = get_seconds_since_1970();
    err = sfs_inodes_db_append(md, &sin, &inode_no);
    if (err) return err;

    // add the entry in the directory
    err = sfs_node_dir_add_entry(md, parent->inode_num, parent_sin, name, inode_no);
    if (err) return err;

    *out = inodes.create(parent->sb, inode_no, parent, name, sin.file_size);
    return OK;
}

static error_t sfs_driver_unlink(inode_t *parent, const char *name) {
    sfs_mount_data *md = (sfs_mount_data *)parent->sb->driver_priv_data;
    uint32_t inode_no;
    uint32_t rec_no;
    error_t err;

    if (is_name_reserved(name))
        return traceable(ERR_INVALID_ARGS);
    
    // find our container inode
    stored_inode *parent_sin;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_sin);
    if (err) return err;
    if (!stored_inode_is_dir(parent_sin)) return traceable(ERR_NOT_A_DIRECTORY);

    // find the target entry
    err = sfs_node_dir_find_entry(md, parent_sin, name, &rec_no, &inode_no);
    if (err) return err;

    // load the target file, ensure it's a file
    stored_inode *dying_sin;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&dying_sin);
    if (err) return err;
    if (!stored_inode_is_file(dying_sin)) return traceable(ERR_NOT_A_FILE);
    
    // first, remove entry from parent directory
    err = sfs_node_dir_set_entry(md, parent->inode_num, parent_sin, rec_no, "", INVALID_INODE_NO);
    if (err) return err;
    
    // then, release file's data blocks
    err = sfs_node_release_all_data_blocks(md, inode_no);
    if (err) return err;

    // finally, invalidate the inode
    dying_sin->type_perms = 0;
    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_no);

    return OK;
}

static error_t sfs_driver_stat(inode_t *n, vfs_stat_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&sin);
    if (err) return err;

    out->st_atime1 = 0;
    out->st_blksize = md->superblock->block_size_in_bytes;
    out->st_blocks = sin->allocated_blocks;
    out->st_ctime1 = sin->modified_at;
    out->st_dev = n->sb->fs_id;
    out->st_gid = sin->group_id;
    out->st_ino = n->inode_num;
    out->st_mode = sin->type_perms;
    out->st_mtime1 = sin->modified_at;
    out->st_nlink = 1; // we don't support multilink
    out->st_size = n->size;
    out->st_uid = sin->user_id;

    return OK;
}

static error_t sfs_driver_truncate(inode_t *n, size_t size) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *sin;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&sin);
    if (err) return err;

    if (size == sin->file_size)
        return OK;

    if (size == 0) {
        err = sfs_node_release_all_data_blocks(md, n->inode_num);
        if (err) return err;
        sin->file_size = 0;
        md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
        return OK;
    }

    if (size > sin->file_size) {
        uint32_t block_size = md->superblock->block_size_in_bytes;
        uint64_t new_size = size;
        while (sin->file_size < new_size) {
            err = sfs_node_expand_data_blocks(md, sin, n->inode_num);
            if (err) return err;
            sin->file_size += block_size;
        }
        sin->file_size = new_size;
        md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
        return OK;
    }

    // shrinking to a non-zero size is not supported
    return traceable(ERR_NOT_SUPPORTED);
}


static fs_driver_ops_t simple_fs_ops = {
    .probe        = sfs_driver_probe,
    .mount        = sfs_driver_mount,
    .unmount      = sfs_driver_unmount,
    .sync         = sfs_driver_sync,
    .mkfs         = sfs_driver_mkfs,
    .get_root_dir = sfs_driver_get_root_dir,
    .lookup       = sfs_driver_lookup,
    .open         = sfs_driver_open,
    .close        = sfs_driver_close,
    .read         = sfs_driver_read,
    .write        = sfs_driver_write,
    .flush        = sfs_driver_flush,
    .opendir      = sfs_driver_opendir,
    .readdir      = sfs_driver_readdir,
    .rewinddir    = sfs_driver_rewinddir,
    .closedir     = sfs_driver_closedir,
    .create       = sfs_driver_create,
    .unlink       = sfs_driver_unlink,
    .mkdir        = sfs_driver_mkdir,
    .rmdir        = sfs_driver_rmdir,
    .stat         = sfs_driver_stat,
    .truncate     = sfs_driver_truncate,
};

fs_driver_t simple_fs = {
    .name = "Simple file system (SFS)",
    .ops = &simple_fs_ops,
    .probe = sfs_driver_probe,
    .mkfs = sfs_driver_mkfs,
};
