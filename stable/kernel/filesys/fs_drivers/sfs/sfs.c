#include "../../fs_driver.h"
#include "../../../include/uapi/errors.h"
#include "../../../memory/kheap.h"
#include "../../../drivers/clock.h"
#include "../../../klib/string.h"
#include "../../../klib/cache.h"
#include "../../../klib/list.h"
#include "../../../logger/logger.h"
#include "sfs_internal.h"


static bool sb_recognized(block_device_t *dev) {

    char *buffer = kmalloc(dev->block_size);
    error_t err = dev->ops->read_sectors(dev, 0, 1, buffer);
    if (err) { log_error("sb_recognized(): error %d (%s) reading the first sector", err, strerror(err)); kfree(buffer); return false; }
    
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
    error_t err = sfs_create_fs_mount_data(sb->dev, &md);
    if (err) return traceable(err);

    sb->driver_priv_data = md;
    return OK;
}

static error_t sfs_driver_unmount(superblock_t *sb) {
    log_trace("sfs_driver_unmount()");
    sfs_mount_data *fs_data = (sfs_mount_data *)sb->driver_priv_data;

    error_t err = sfs_sync_fs_mount_data(fs_data);
    if (err) return traceable(err);

    sfs_destroy_fs_mount_data(fs_data);
    sb->driver_priv_data = NULL;
    return OK;
}

static error_t sfs_driver_sync(superblock_t *sb) {
    log_trace("sfs_driver_sync()");
    return sfs_sync_fs_mount_data((sfs_mount_data *)sb->driver_priv_data);
}

static size_t auto_determine_block_size(uint32_t sector_size, uint64_t dev_capacity) {
    uint32_t block_size = 0;

    if      (dev_capacity <=  2 * MB) block_size =  512;
    else if (dev_capacity <=  8 * MB) block_size = 1024;
    else if (dev_capacity <= 32 * MB) block_size = 2048;
    else                              block_size = MAX_BLOCK_SIZE_BYTES;
    
    if (block_size < sector_size)
        block_size = sector_size;

    return block_size;
}

static size_t auto_determine_inodes_count(uint64_t dev_capacity, size_t block_size) {
    // assuming 8-16kB average file size, divide capacity to find inode count
    size_t estimated_average_file_size = 8 * 1024; 

    uint32_t inodes_count = (uint32_t)(dev_capacity / estimated_average_file_size);
    // min to 512 for small drives
    inodes_count = inodes_count < 512 ? 512 : inodes_count;

    // round up to take whole blocks
    size_t inodes_per_block = block_size / sizeof(stored_inode);
    inodes_count = ((inodes_count + inodes_per_block - 1) / inodes_per_block) * inodes_per_block;
    
    // scenarios: 8MB  -> 1024 inodes -> 64K bytes -> 0.7% storage
    //            32MB -> 4096 inodes -> 256K bytes -> 0.7% storage
    return inodes_count;
}

static inline error_t write_fs_block(block_device_t *dev, uint32_t sectors_per_block, uint32_t block_no, void *buffer) {
    return dev->ops->write_sectors(dev, block_no * sectors_per_block, sectors_per_block, buffer);
}

static error_t sfs_driver_mkfs(block_device_t *dev) {
    error_t err;
    stored_superblock *sb = NULL;
    bitmap_t *blocks_bitmap = NULL;
    bitmap_t *inodes_bitmap = NULL;

    uint64_t dev_capacity_bytes = dev->block_size * dev->total_blocks;
    uint32_t fs_block_size = auto_determine_block_size(dev->block_size, dev_capacity_bytes);
    uint32_t num_inodes = auto_determine_inodes_count(dev_capacity_bytes, dev->block_size);
    uint32_t bsects = fs_block_size / dev->block_size;

    // --- superblock ---
    sb = kmalloc(sizeof(stored_superblock));
    memset(sb, 0, sizeof(stored_superblock));
    memcpy(sb->magic, "SFS1", sizeof(sb->magic));
    sb->direntry_size = sizeof(stored_dir_entry);
    sb->inode_size = sizeof(stored_inode);
    sb->num_inodes = num_inodes;
    sb->block_size_in_bytes = fs_block_size;
    sb->sector_size = dev->block_size;
    sb->sectors_per_block = bsects;
    sb->blocks_in_device = dev->total_blocks / sb->sectors_per_block;
    sb->inodes_per_block = fs_block_size / sizeof(stored_inode);
    sb->ranges_per_block = fs_block_size / sizeof(block_range);
    strscpy(sb->volume_label, "SFS_VOL", sizeof(sb->volume_label));

    // metrics
    sb->inodes_bitmap_first_block = 1; // following the superblock
    sb->inodes_bitmap_num_blocks = DIV_CEIL(DIV_CEIL(num_inodes, 8), fs_block_size);
    sb->inodes_array_first_block = sb->inodes_bitmap_first_block + sb->inodes_bitmap_num_blocks;
    sb->inodes_array_num_blocks = (num_inodes * sizeof(stored_inode)) / fs_block_size;
    sb->blocks_bitmap_first_block = sb->inodes_array_first_block + sb->inodes_array_num_blocks;
    sb->blocks_bitmap_num_blocks = DIV_CEIL(DIV_CEIL(sb->blocks_in_device, 8), fs_block_size);

    // create block bitmaps, mark used blocks
    blocks_bitmap = create_bitmap(sb->blocks_in_device, sb->blocks_bitmap_num_blocks);
    blocks_bitmap->ops->mark_used(blocks_bitmap, 0); // superblock
    for (uint32_t i = 0; i < sb->inodes_bitmap_num_blocks; i++) blocks_bitmap->ops->mark_used(blocks_bitmap, sb->inodes_bitmap_first_block + i);
    for (uint32_t i = 0; i < sb->inodes_array_num_blocks;  i++) blocks_bitmap->ops->mark_used(blocks_bitmap, sb->inodes_array_first_block  + i);
    for (uint32_t i = 0; i < sb->blocks_bitmap_num_blocks; i++) blocks_bitmap->ops->mark_used(blocks_bitmap, sb->blocks_bitmap_first_block + i);

    // create inode bitmaps, mark used inodes
    inodes_bitmap = create_bitmap(sb->num_inodes, sb->inodes_bitmap_num_blocks);
    inodes_bitmap->ops->mark_used(inodes_bitmap, 0);                 // the invalid inode
    inodes_bitmap->ops->mark_used(inodes_bitmap, ROOT_DIR_INODE_ID); // the root inode

    // block buffer used to save things
    char *block_buffer = kmalloc(fs_block_size);
    memset(block_buffer, 0, fs_block_size);

    // create and save superblock
    memset(block_buffer, 0, fs_block_size);
    memcpy(block_buffer, sb, sizeof(stored_superblock));
    err = write_fs_block(dev, bsects, 0, block_buffer);
    if (err < 0) goto cleanup;

    // save zeros for all bitmaps and inodes
    memset(block_buffer, 0, fs_block_size);
    for (uint32_t i = 0; i < sb->inodes_bitmap_num_blocks; i++) { if ((err = write_fs_block(dev, bsects, sb->inodes_bitmap_first_block + i, block_buffer)) != OK) goto cleanup; }
    for (uint32_t i = 0; i < sb->inodes_array_num_blocks;  i++) { if ((err = write_fs_block(dev, bsects, sb->inodes_array_first_block  + i, block_buffer)) != OK) goto cleanup; }
    for (uint32_t i = 0; i < sb->blocks_bitmap_num_blocks; i++) { if ((err = write_fs_block(dev, bsects, sb->blocks_bitmap_first_block + i, block_buffer)) != OK) goto cleanup; }

    // create & save root dir inode
    uint32_t first_data_block_no = 0;
    if (!blocks_bitmap->ops->find_next_free(blocks_bitmap, &first_data_block_no)) return traceable(ERR_NO_SPACE_LEFT);
    blocks_bitmap->ops->mark_used(blocks_bitmap, first_data_block_no);
    memset(block_buffer, 0, fs_block_size);
    stored_inode *inodes = (stored_inode *)block_buffer;
    inodes[ROOT_DIR_INODE_ID] = new_stored_inode_dir();
    inodes[ROOT_DIR_INODE_ID].type_perms |= 0755; // set to "rwxr-xr-x" by default
    inodes[ROOT_DIR_INODE_ID].allocated_blocks = 1;
    inodes[ROOT_DIR_INODE_ID].ranges[0].first_block_no = first_data_block_no;
    inodes[ROOT_DIR_INODE_ID].ranges[0].blocks_count = 1;
    inodes[ROOT_DIR_INODE_ID].created_at = get_seconds_since_1970();
    inodes[ROOT_DIR_INODE_ID].file_size = 2 * sizeof(stored_dir_entry);

    err = write_fs_block(dev, bsects, sb->inodes_array_first_block, inodes);
    if (err < 0) goto cleanup;
    
    // save root dir data contents
    memset(block_buffer, 0, fs_block_size);
    stored_dir_entry *entries = (stored_dir_entry *)block_buffer;
    strcpy(entries[0].name, ".");
    entries[0].inode_num = ROOT_DIR_INODE_ID;
    strcpy(entries[1].name, "..");
    entries[1].inode_num = ROOT_DIR_INODE_ID;
    err = write_fs_block(dev, bsects, first_data_block_no, entries);
    if (err < 0) goto cleanup;

    // save inodes bitmap, since at least one allocation
    char *ptr = (char *)blocks_bitmap->words;
    for (size_t i = 0; i < sb->inodes_bitmap_num_blocks; i++) {
        err = write_fs_block(dev, bsects, sb->inodes_bitmap_first_block + i, ptr);
        if (err) goto cleanup;
        ptr += sb->block_size_in_bytes;
    }

    // save blocks bitmap
    ptr = (char *)blocks_bitmap->words;
    for (size_t i = 0; i < sb->blocks_bitmap_num_blocks; i++) {
        err = write_fs_block(dev, bsects, sb->blocks_bitmap_first_block + i, ptr);
        if (err) goto cleanup;
        ptr += sb->block_size_in_bytes;
    }

    // we got here, we are good
    err = OK;
cleanup:
    kfree(block_buffer);
    kfree(sb);
    if (blocks_bitmap) blocks_bitmap->ops->destroy(blocks_bitmap);
    if (inodes_bitmap) inodes_bitmap->ops->destroy(inodes_bitmap);
    return traceable(err);
}

static error_t sfs_driver_get_root_dir(superblock_t *sb, inode_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)sb->driver_priv_data;
    stored_inode root_inode;
    error_t err = sfs_load_inode2(md, ROOT_DIR_INODE_ID, &root_inode);
    if (err) return traceable(err);
    *out = inodes.create(sb, ROOT_DIR_INODE_ID, true, false, root_inode.file_size);
    return OK;
}

static error_t sfs_driver_lookup(inode_t *dir, const char *name, inode_t *out) {
    log_trace("sfs_driver_lookup(dir=%llu, name='%s')", dir->inode_num, name);
    sfs_mount_data *md = (sfs_mount_data *)dir->sb->driver_priv_data;
    stored_inode *parent_inode;
    stored_inode *target_inode;
    error_t err;

    err = md->inode_cache->ops->get(md->inode_cache, dir->inode_num, (void **)&parent_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    uint32_t rec_no;
    uint32_t target_inode_no;
    err = sfs_find_direntry(md, parent_inode, name, &rec_no, &target_inode_no);
    if (err) return traceable(err);
    
    err = md->inode_cache->ops->get(md->inode_cache, target_inode_no, (void **)&target_inode);
    if (err) return traceable(err);

    *out = inodes.create(dir->sb, target_inode_no, STORED_INODE_IS_DIR(target_inode), STORED_INODE_IS_FILE(target_inode), target_inode->file_size);
    return OK;
}

static error_t sfs_driver_open(inode_t *n, int flags, open_file_t **file_handle) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;

    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&inode);
    if (err) return traceable(err);

    // Validate that it's a file
    if (!stored_inode_is_file(inode)) {
        // If it's a directory, allow opening only for read operations
        if (stored_inode_is_dir(inode) && (flags & (O_WRONLY | O_RDWR))) {
            return traceable(ERR_IS_A_DIRECTORY);
        }
        // For other non-file types (e.g., block device, char device) or invalid access, return traceable(err)or
        else if (!stored_inode_is_dir(inode)) {
             return traceable(ERR_NOT_A_FILE);
        }
    }

    *file_handle = open_files.create(n->sb, n, flags);

    // ensure this is not evicted until file is closed
    err = md->inode_cache->ops->lock(md->inode_cache, n->inode_num);
    if (err) {
        open_files.release(*file_handle); // Release the created open_file_t if locking fails
        return traceable(err);
    }

    // No need to handle O_TRUNC here, as VFS layer handles it via vfs_truncate before this call.
    // No need to handle O_CREAT/O_EXCL here, as VFS layer handles file creation before this call.
    // O_APPEND is handled by VFS layer in vfs_write.

    return OK;
}

static error_t sfs_driver_close(open_file_t *file) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    error_t err;

    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    
    stored_inode *inode;
    err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&inode);
    if (err) return traceable(err);

    // inode->modified_at = ...
    
    err = md->inode_cache->ops->flush(md->inode_cache, file->inode.inode_num);
    if (err) return traceable(err);
    err = md->inode_cache->ops->unlock(md->inode_cache, file->inode.inode_num);
    if (err) return traceable(err);
    
    return OK;
}

static ssize_t sfs_driver_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    stored_inode *inode;
    
    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&inode);
    if (err) return traceable(err);

    ssize_t answer = sfs_read_file_data(md, inode, offset, buf, len);
    return answer;
}

static ssize_t sfs_driver_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;
    stored_inode *inode;
    
    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->get(md->inode_cache, file->inode.inode_num, (void **)&inode);
    if (err) return traceable(err);

    ssize_t answer = sfs_write_file_data(md, inode, offset, buf, len);
    md->inode_cache->ops->mark_dirty(md->inode_cache, file->inode.inode_num);
    return answer;
}

static error_t sfs_driver_flush(open_file_t *file) {
    sfs_mount_data *md = (sfs_mount_data *)file->sb->driver_priv_data;

    // TODO: we need a function to walk all blocks of the inode and call "flush" on the cache of them
    if (!md->inode_cache->ops->is_locked(md->inode_cache, file->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    error_t err = md->inode_cache->ops->flush(md->inode_cache, file->inode.inode_num);
    if (err) return traceable(err);
    
    return OK;
}

static error_t sfs_driver_opendir(inode_t *dir, open_file_t **dir_handle) {
    sfs_mount_data *md = (sfs_mount_data *)dir->sb->driver_priv_data;

    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, dir->inode_num, (void **)&inode);
    if (err) return traceable(err);

    // validate first
    if (!stored_inode_is_dir(inode)) return traceable(ERR_NOT_A_DIRECTORY);

    *dir_handle = open_files.create(dir->sb, dir, O_RDONLY);

    // ensure this is not evicted until dir is closed
    err = md->inode_cache->ops->lock(md->inode_cache, dir->inode_num);
    if (err) return traceable(err);

    return OK;
}

static ssize_t sfs_driver_readdir(open_file_t *dir_handle, vfs_dirent_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)dir_handle->sb->driver_priv_data;
    error_t err;
    stored_inode *dir_inode; // The inode of the directory being read.

    if (!md->inode_cache->ops->is_locked(md->inode_cache, dir_handle->inode.inode_num))
        return traceable(ERR_BAD_FILE);
    err = md->inode_cache->ops->get(md->inode_cache, dir_handle->inode.inode_num, (void **)&dir_inode);
    if (err) return traceable(err);

    // 'dir_handle->offset' now serves as the physical byte offset within the
    // directory's raw data blocks where the driver should start searching
    // for the next valid 'stored_dir_entry'.
    // This offset is managed by the driver and aligns with the concept of
    // an opaque stream position (cookie) for readdir operations.
    uint32_t current_physical_scan_offset = dir_handle->offset;

    // Loop to find the next valid directory entry from the current scan offset.
    while (current_physical_scan_offset < dir_inode->file_size) {
        stored_dir_entry sde;
        ssize_t bytes_read_from_physical;

        // Read one 'stored_dir_entry' from the current physical scan offset.
        bytes_read_from_physical = sfs_read_file_data(
            md, dir_inode, current_physical_scan_offset, &sde, sizeof(stored_dir_entry)
        );

        if (bytes_read_from_physical <= 0) {
            // Error during read or end of directory's physical data.
            // Update 'dir_handle->offset' to mark EOF for subsequent calls.
            dir_handle->offset = dir_inode->file_size;
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
        strscpy(out->d_name, sde.name, sizeof(out->d_name) - 1);
        out->d_name[sizeof(out->d_name) - 1] = '\0'; // Ensure null-termination
        out->d_ino = sde.inode_num;

        // Determine the type of the entry's inode.
        stored_inode *entry_inode;
        err = md->inode_cache->ops->get(md->inode_cache, sde.inode_num, (void **)&entry_inode);
        if (err) {
            // If inode lookup fails, it might be a deleted inode or corrupted.
            // Report as a generic file type.
            out->d_type = S_IFREG;
        } else {
            out->d_type = (entry_inode->type_perms & STORED_INODE_TYPE_MASK);
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
    dir_handle->offset = dir_inode->file_size;
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
    if (err) return traceable(err);
    
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
    stored_inode *parent_inode;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    // see if already exists
    err = sfs_find_direntry(md, parent_inode, name, &rec_no, &inode_no);
    if (err == OK) return traceable(ERR_ALREADY_EXISTS);

    stored_inode initialized_inode = new_stored_inode_dir();
    initialized_inode.created_at = get_seconds_since_1970();

    // allocate new inode num and save
    err = sfs_allocate_inode2(md, &inode_no);
    if (err) return traceable(err);
    err = sfs_save_inode2(md, inode_no, &initialized_inode);
    if (err) return traceable(err);

    // add the entry in the parent directory
    err = sfs_add_direntry(md, parent_inode, name, inode_no, &rec_no);
    if (err) return traceable(err);

    // load our new directory from disk
    stored_inode *created_inode;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&created_inode);
    if (err) return traceable(err);

    // we now need to add the two references, to self and parent.
    err = sfs_add_direntry(md, created_inode, ".", inode_no, &rec_no);
    if (err) return traceable(err);
    err = sfs_add_direntry(md, created_inode, "..", parent->inode_num, &rec_no);
    if (err) return traceable(err);

    *out = inodes.create(parent->sb, inode_no, true, false, created_inode->file_size);
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
    stored_inode *parent_inode;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    // find the target entry
    err = sfs_find_direntry(md, parent_inode, name, &rec_no, &inode_no);
    if (err) return traceable(err);

    // load the target dir, to verify if empty
    stored_inode *dying_inode;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&dying_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(dying_inode)) return traceable(ERR_NOT_A_DIRECTORY);
    
    // see if it is not empty, ignoring special entries
    bool is_empty = false;
    err = sfs_check_dir_emptiness(md, dying_inode, &is_empty);
    if (err) return traceable(err);
    if (!is_empty) return traceable(ERR_DIR_NOT_EMPTY);

    // first, remove entry from parent directory
    err = sfs_clear_direntry(md, parent_inode, rec_no);
    if (err) return traceable(err);

    // then, release directory's own data blocks
    err = sfs_node_num_release_all_data_blocks(md, inode_no);
    if (err) return traceable(err);

    // finally, invalidate the inode
    dying_inode->type_perms = 0;
    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_no);

    return OK;
}

static error_t sfs_driver_create(inode_t *parent, const char *name, int type, uid_t uid, gid_t gid, inode_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)parent->sb->driver_priv_data;
    uint32_t inode_no;
    uint32_t rec_no;
    error_t err;

    if (name == NULL || strlen(name) == 0 || strchr(name, '/') != NULL || is_name_reserved(name))
        return traceable(ERR_INVALID_ARGS);
    
    // find our inode entry
    stored_inode *parent_inode;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    // see if already exists
    err = sfs_find_direntry(md, parent_inode, name, &rec_no, &inode_no);
    if (err == OK) return traceable(ERR_ALREADY_EXISTS);

    // SFS only supports 8-bit user and group ids
    ASSERT((uid & ~0xFF) == 0);
    ASSERT((gid & ~0xFF) == 0);

    // create inode on disk (cache cannot do this)
    stored_inode new_inode = new_stored_inode_file();
    new_inode.type_perms = S_IFREG | 0666;
    new_inode.user_id = uid;
    new_inode.group_id = gid;
    new_inode.created_at = get_seconds_since_1970();

    // allocate new inode num and save
    err = sfs_allocate_inode2(md, &inode_no);
    if (err) return traceable(err);
    err = sfs_save_inode2(md, inode_no, &new_inode);
    if (err) return traceable(err);

    // add the entry in the directory
    err = sfs_add_direntry(md, parent_inode, name, inode_no, &rec_no);
    if (err) return traceable(err);

    *out = inodes.create(parent->sb, inode_no, parent, name, new_inode.file_size);
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
    stored_inode *parent_inode;
    err = md->inode_cache->ops->get(md->inode_cache, parent->inode_num, (void **)&parent_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_dir(parent_inode)) return traceable(ERR_NOT_A_DIRECTORY);

    // find the target entry
    err = sfs_find_direntry(md, parent_inode, name, &rec_no, &inode_no);
    if (err) return traceable(err);

    // load the target file, ensure it's a file
    stored_inode *dying_inode;
    err = md->inode_cache->ops->get(md->inode_cache, inode_no, (void **)&dying_inode);
    if (err) return traceable(err);
    if (!stored_inode_is_file(dying_inode)) return traceable(ERR_NOT_A_FILE);
    
    // first, remove entry from parent directory
    err = sfs_clear_direntry(md, parent_inode, rec_no);
    if (err) return traceable(err);
    
    // then, release file's data blocks
    err = sfs_node_num_release_all_data_blocks(md, inode_no);
    if (err) return traceable(err);

    // finally, invalidate the inode
    dying_inode->type_perms = 0;
    md->inode_cache->ops->mark_dirty(md->inode_cache, inode_no);

    return OK;
}

static error_t sfs_driver_stat(inode_t *n, vfs_stat_t *out) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&inode);
    if (err) return traceable(err);

    out->st_atime1 = 0;
    out->st_blksize = md->superblock->block_size_in_bytes;
    out->st_blocks = inode->allocated_blocks;
    out->st_ctime1 = inode->modified_at;
    out->st_dev = n->sb->fs_id;
    out->st_gid = inode->group_id;
    out->st_ino = n->inode_num;
    out->st_mode = inode->type_perms;
    out->st_mtime1 = inode->modified_at;
    out->st_nlink = 1; // we don't support multilink
    out->st_size = n->size;
    out->st_uid = inode->user_id;

    return OK;
}

static error_t sfs_driver_chmod(inode_t *n, uint32_t mode) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&inode);
    if (err) return traceable(err);

    inode->type_perms = (inode->type_perms & ~S_IRWXUGO) | (mode & S_IRWXUGO); // Preserve file type, update permissions
    md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
    return OK;
}

static error_t sfs_driver_chown(inode_t *n, uid_t uid, gid_t gid) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&inode);
    if (err) return traceable(err);

    inode->user_id = uid;
    inode->group_id = gid;
    md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
    return OK;
}

static error_t sfs_driver_truncate(inode_t *n, size_t size) {
    sfs_mount_data *md = (sfs_mount_data *)n->sb->driver_priv_data;
    stored_inode *inode;
    error_t err = md->inode_cache->ops->get(md->inode_cache, n->inode_num, (void **)&inode);
    if (err) return traceable(err);

    if (size == inode->file_size)
        return OK;

    if (size == 0) {
        err = sfs_node_num_release_all_data_blocks(md, n->inode_num);
        if (err) return traceable(err);
        inode->file_size = 0;
        md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
        return OK;
    }

    if (size < inode->file_size) {
        inode->file_size = size;
        return OK;
    }

    if (size > inode->file_size) {
        uint32_t block_size = md->superblock->block_size_in_bytes;
        while (inode->file_size < size) {
            err = sfs_node_expand_data_blocks(md, inode);
            if (err) return traceable(err);
            inode->file_size += block_size;
        }
        inode->file_size = size;
        md->inode_cache->ops->mark_dirty(md->inode_cache, n->inode_num);
        return OK;
    }

    // shrinking to a non-zero size is not supported
    return traceable(ERR_NOT_SUPPORTED);
}


static fs_driver_ops_t console_fs_ops = {
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
    .chmod        = sfs_driver_chmod,
    .chown        = sfs_driver_chown,
    .ioctl        = NULL,
};

fs_driver_t simple_fs = {
    .name = "Simple file system (SFS)",
    .ops = &console_fs_ops,
    .probe = sfs_driver_probe,
    .mkfs = sfs_driver_mkfs,
};
