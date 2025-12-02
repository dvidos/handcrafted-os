#include "../dependencies/returns.h"
#include "../simple_filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

// ------------------------------------------------------------------

#include "path.inc.c"
#include "superblock.inc.c"
#include "bitmap.inc.c"
#include "cache.inc.c"
#include "ranges.inc.c"
#include "block_ops.inc.c"
#include "inodes.inc.c"
#include "open.inc.c"
#include "dirs.inc.c"
#include "resolution.inc.c"

// ------------------------------------------------------------------

static int sfs_mkfs(simple_filesystem *sfs, char *volume_label, uint32_t desired_block_size) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted != NULL)
        return ERR_NOT_SUPPORTED;

    // prepare superblock for block 0
    superblock *sb = data->memory->allocate(data->memory, sizeof(superblock));
    int err = populate_superblock(
        volume_label,
        data->device->get_sector_size(data->device),
        data->device->get_sector_count(data->device),
        desired_block_size,
        sb
    );
    if (err != OK) return err;

    // for blocks 1+, we need the bitmap
    mounted_data *mt = data->memory->allocate(data->memory, sizeof(mounted_data));
    mt->superblock = sb;
    mt->used_blocks_bitmap = data->memory->allocate(data->memory, sb->blocks_bitmap_blocks_count * sb->block_size_in_bytes);

    mark_all_blocks_free(mt);
    mark_block_used(mt, 0);
    for (int i = 0; i < sb->blocks_bitmap_blocks_count; i++)
        mark_block_used(mt, sb->blocks_bitmap_first_block + i);

    // we can start writing, let's grab a cache
    mt->cache = initialize_cache(data->memory, data->device, sb->block_size_in_bytes);

    err = cached_write(mt->cache, 0, 0, sb, sizeof(superblock));
    if (err != OK) return err;
    err = used_blocks_bitmap_save(mt);
    if (err != OK) return err;

    // persist everything and we're done
    err = cache_flush(mt->cache);
    if (err != OK) return err;

    return OK;
}

static int sfs_mount(simple_filesystem *sfs, int readonly) {
    int err;
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted != NULL)
        return ERR_NOT_SUPPORTED;

    // first check the first sector to see if we can mount it.
    // hence why the superblock must be at most 512 bytes size.
    uint8_t *temp_sector = data->memory->allocate(data->memory, data->device->get_sector_size(data->device));
    err = data->device->read_sector(data->device, 0, temp_sector);
    if (err != OK) return err;
    superblock *sb = (superblock *)temp_sector;

    // basic verification
    if (memcmp(sb->magic, "SFS1", 4) != 0) {
        data->memory->release(data->memory, temp_sector);
        return ERR_NOT_RECOGNIZED;
    }
    if (sb->inode_size != sizeof(inode) || sb->direntry_size != sizeof(direntry)) {
        data->memory->release(data->memory, temp_sector);
        return ERR_NOT_SUPPORTED;
    }

    // initialize structures based on superblock's numbers
    mounted_data *mt = data->memory->allocate(data->memory, sizeof(mounted_data));
    memset(mt, 0, sizeof(mounted_data));
    mt->readonly = readonly;
    mt->superblock = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mt->used_blocks_bitmap = data->memory->allocate(data->memory, sb->blocks_bitmap_blocks_count * sb->block_size_in_bytes);
    mt->cache = initialize_cache(data->memory, data->device, sb->block_size_in_bytes);
    mt->generic_block_buffer = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mt->clock = data->clock;
    data->mounted = mt;
    
    // we can release temp sector now
    data->memory->release(data->memory, temp_sector);

    // cache can now read the main blocks
    err = cached_read(mt->cache, 0, 0, mt->superblock, sizeof(superblock));
    if (err != OK) return err;
    err = used_blocks_bitmap_load(mt);
    if (err != OK) return err;

    // we should make open the two special inodes (offset 0 and 1)
    err = open_files_register(mt, &mt->superblock->inodes_db_inode, INODE_DB_INDDE_ID, NULL);
    if (err != OK) return err;
    err = open_files_register(mt, &mt->superblock->root_dir_inode, ROOT_DIR_INODE_ID, NULL);
    if (err != OK) return err;

    return OK;
}

static int sfs_sync(simple_filesystem *sfs) {
    int err;

    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt->readonly)
        return ERR_NOT_PERMITTED;

    // save open inodes to superblock or to inodes db
    err = open_files_flush_dirty_inodes(mt);
    if (err != OK) return err;
    
    // write superblock to cache
    err = cached_write(mt->cache, 0, 0, mt->superblock, sizeof(superblock));
    if (err != OK) return err;

    // write used blocks bitmap to disk (blocks 1+)
    err = used_blocks_bitmap_save(mt);
    if (err != OK) return err;

    // flush all caches (includes SB, bitmap, inodesdb, root dir, file blocks)
    err = cache_flush(data->mounted->cache);
    if (err != OK) return err;

    return OK;
}

static int sfs_unmount(simple_filesystem *sfs) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;

    if (!data->mounted->readonly) {
        int err = sfs_sync(sfs);
        if (err != OK) return err;
    }

    cache_release_memory(data->mounted->cache);
    data->memory->release(data->memory, data->mounted->superblock);
    data->memory->release(data->memory, data->mounted->used_blocks_bitmap);
    data->memory->release(data->memory, data->mounted->generic_block_buffer);
    data->memory->release(data->memory, data->mounted);
    data->mounted = NULL;

    return OK;
}

static int sfs_open(simple_filesystem *sfs, char *filename, int options, sfs_handle **handle_ptr) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    int err;
    
    // see if path resolves to inode
    inode inode;
    uint32_t inode_id;
    err = resolve_path_to_inode(mt, filename, &inode, &inode_id);
    if (err != OK) return err;
    if (!inode.is_file)
        return ERR_WRONG_TYPE;

    // get open file handle, open inode if needed
    open_handle *handle;
    err = open_files_register(mt, &inode, inode_id, &handle);
    if (err != OK) return err;

    *handle_ptr = (sfs_handle *)handle;
    return OK;
}

static int sfs_read(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;

    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    int bytes_read = inode_read_file_bytes(mt, &handle->inode->inode_in_mem, handle->file_position, buffer, size);
    if (bytes_read < 0) return bytes_read; // error

    handle->file_position += bytes_read;
    return bytes_read;
}

static int sfs_write(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (mt->readonly) return ERR_NOT_PERMITTED;

    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    // this call takes care of file_size as well
    int bytes_written = inode_write_file_bytes(mt, &handle->inode->inode_in_mem, handle->file_position, buffer, size);
    if (bytes_written < 0) return bytes_written; // error

    handle->file_position += bytes_written;
    handle->inode->is_dirty = 1;
    return bytes_written;
}

static int sfs_close(simple_filesystem *sfs, sfs_handle *h) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    
    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    int err = open_handles_release(mt, handle);
    if (err != OK) return err;

    return OK;
}

static int sfs_seek(simple_filesystem *sfs, sfs_handle *h, int offset, int origin) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    
    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    // if sizes are more than what ints can support, this function is erroneous / dangerous / useless
    if (handle->file_position >= 0x7FFFFFFF || handle->inode->inode_in_mem.file_size >= 0x7FFFFFFF)
        return ERR_NOT_SUPPORTED;
    
    int pos = (int)handle->file_position;
    int size = (int)handle->inode->inode_in_mem.file_size;
    int remain = size - pos;

    if (origin == 0) {
        // from start
        pos = in_range(offset, 0, size);

    } else if (origin == 1) {
        // from curr position
        pos = pos + in_range(offset, pos*-1, remain);

    } else if (origin == 2) {
        // from end
        pos = size + in_range(offset, size*-1, 0);

    } else {
        return ERR_INVALID_ARGUMENT;
    }

    handle->file_position = pos;
    return OK;
}

static int sfs_tell(simple_filesystem *sfs, sfs_handle *h) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    
    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    return handle->file_position;
}

static int sfs_open_dir(simple_filesystem *sfs, char *path, sfs_handle **handle_ptr) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    int err;
    
    // see if path resolves to inode
    inode inode;
    uint32_t inode_id;
    err = resolve_path_to_inode(mt, path, &inode, &inode_id);
    if (err != OK) return err;
    if (!inode.is_dir)
        return ERR_WRONG_TYPE;

    // get open file handle, open inode if needed
    open_handle *handle;
    err = open_files_register(mt, &inode, inode_id, &handle);
    if (err != OK) return err;

    *handle_ptr = (sfs_handle *)handle;
    return OK;
}

static int sfs_read_dir(simple_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;

    // we have the handle->curr_position, let's make it read dir entries
    // that's it.
    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used || !handle->inode->inode_in_mem.is_dir)
        return ERR_INVALID_ARGUMENT;
    
    if (handle->file_position >= handle->inode->inode_in_mem.file_size)
        return ERR_END_OF_FILE;

    // else read, populate, advance file position.
    direntry disk_entry;
    int bytes = inode_read_file_bytes(mt, &handle->inode->inode_in_mem, handle->file_position, &disk_entry, sizeof(direntry));
    if (bytes < 0) return bytes;
    handle->file_position += bytes;

    memset(entry, 0, sizeof(sfs_dir_entry));
    strncpy(entry->name, disk_entry.name, sizeof(entry->name)); // 60
    entry->name[sizeof(disk_entry.name)] = 0; // ideally 61 out of 60
    // we might give more attributes, but for now, this'll do.
    
    return OK;
}

static int sfs_close_dir(simple_filesystem *sfs, sfs_handle *h) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    
    open_handle *handle = (open_handle *)h;
    if (handle == NULL || handle->inode == NULL || !handle->is_used)
        return ERR_INVALID_ARGUMENT;

    int err = open_handles_release(mt, handle);
    if (err != OK) return err;

    return OK;
}

static int sfs_stat(simple_filesystem *sfs, char *path, sfs_stat_info *info) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    int err;

    // see if path resolves to inode
    inode target_inode;
    uint32_t target_inode_id;
    err = resolve_path_to_inode(mt, path, &target_inode, &target_inode_id);
    if (err != OK) return err;
    if (!target_inode.is_file) return ERR_WRONG_TYPE;

    info->inode_id = target_inode_id;
    info->file_size = target_inode.file_size;
    info->type = target_inode.is_file ? 1 : target_inode.is_dir ? 2 : 0;
    info->blocks = target_inode.allocated_blocks;
    info->created_at = 0;
    info->modified_at = 0;
    return OK;
}

static int sfs_create(simple_filesystem *sfs, char *path, int is_dir) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (mt->readonly) return ERR_NOT_PERMITTED;
    int err;
    
    // see if path resolves to inode
    inode parent_inode;
    uint32_t parent_inode_id;
    err = resolve_path_parent_to_inode(mt, path, &parent_inode, &parent_inode_id);
    if (err != OK) return err;

    // find desired name
    const char *desired_name = path_get_last_part(path);
    if (strlen(desired_name) == 0 || strcmp(desired_name, ".") == 0 || strcmp(desired_name, "..") == 0)
        return ERR_INVALID_ARGUMENT;

    // see if name exists in the directory
    err = dir_entry_ensure_missing(mt, &parent_inode, desired_name);
    if (err != OK) return err;

    // now we should be able to create a file / dir
    inode new_inode;
    uint32_t new_inode_id;
    err = inode_allocate(mt, is_dir, &new_inode, &new_inode_id);
    if (err != OK) return err;

    // add this entry
    err = dir_entry_append(mt, &parent_inode, desired_name, new_inode_id);
    if (err != OK) return err;

    // we must persist the parent inode changes (e.g. entries added)
    err = inode_persist(mt, parent_inode_id, &parent_inode);
    if (err != OK) return err;

    // if we added a directory, we need to add the "." and the ".." as well.
    if (is_dir) {
        err = dir_entry_append(mt, &new_inode, ".", new_inode_id);
        if (err != OK) return err;
        err = dir_entry_append(mt, &new_inode, "..", parent_inode_id);
        if (err != OK) return err;
        err = inode_persist(mt, new_inode_id, &new_inode);
        if (err != OK) return err;
    }

    return OK;
}

static int sfs_truncate(simple_filesystem *sfs, char *path) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (mt->readonly) return ERR_NOT_PERMITTED;
    int err;
    
    // see if path resolves to inode
    inode target_inode;
    uint32_t target_inode_id;
    err = resolve_path_to_inode(mt, path, &target_inode, &target_inode_id);
    if (err != OK) return err;
    if (!target_inode.is_file) return ERR_WRONG_TYPE;

    if (target_inode.file_size == 0)
        return OK;
    
    // load this inode, truncate, remove it
    err = inode_truncate_file(mt, &target_inode);
    if (err != OK) return err;

    // we should persist the parent inode changes (maybe change time changed)
    err = inode_persist(mt, target_inode_id, &target_inode);
    if (err != OK) return err;

    return OK;
}

static int sfs_unlink(simple_filesystem *sfs, char *path, int options) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (mt->readonly) return ERR_NOT_PERMITTED;
    int err;
    
    // see if path resolves to inode
    inode parent_inode;
    uint32_t parent_inode_id;
    err = resolve_path_parent_to_inode(mt, path, &parent_inode, &parent_inode_id);
    if (err != OK) return err;

    // see if name exists in the directory
    uint32_t doomed_inode_id;
    uint32_t direntry_rec_no;
    err = dir_entry_find(mt, &parent_inode, path_get_last_part(path), &doomed_inode_id, &direntry_rec_no);
    if (err != OK) return err;

    // now we can unlink this (e.g. nullify dir entry)
    err = dir_entry_delete(mt, &parent_inode, direntry_rec_no);
    if (err != OK) return err;

    // we should persist the parent inode changes (maybe change time changed)
    err = inode_persist(mt, parent_inode_id, &parent_inode);
    if (err != OK) return err;

    // load this inode, truncate, remove it
    inode doomed;
    err = inode_load(mt, doomed_inode_id, &doomed);
    if (err != OK) return err;
    err = inode_truncate_file(mt, &doomed);
    if (err != OK) return err;
    err = inode_delete(mt, doomed_inode_id);
    if (err != OK) return err;

    return OK;
}

static int sfs_rename(simple_filesystem *sfs, char *oldpath, char *newpath) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (mt->readonly) return ERR_NOT_PERMITTED;
    int err;
    
    // fild old containing dir
    inode old_parent_inode;
    uint32_t old_parent_inode_id;
    err = resolve_path_parent_to_inode(mt, oldpath, &old_parent_inode, &old_parent_inode_id);
    if (err != OK) return err;

    // see if name exists in the directory
    uint32_t file_inode_id;
    uint32_t old_entry_no;
    err = dir_entry_find(mt, &old_parent_inode, path_get_last_part(oldpath), &file_inode_id, &old_entry_no);
    if (err != OK) return err;

    // find new containing dir
    inode new_parent_inode;
    uint32_t new_parent_inode_id;
    err = resolve_path_parent_to_inode(mt, newpath, &new_parent_inode, &new_parent_inode_id);
    if (err != OK) return err;

    const char *new_filename = path_get_last_part(newpath);

    // see if name exists in the directory
    err = dir_entry_ensure_missing(mt, &new_parent_inode, new_filename);
    if (err != OK) return err;

    // if same directory, avoid keeping two copies of the inode in memory
    if (old_parent_inode_id == new_parent_inode_id) {
        // we'll update the entry in place
        err = dir_entry_update(mt, &old_parent_inode, old_entry_no, new_filename, file_inode_id);
        if (err != OK) return err;
        err = inode_persist(mt, old_parent_inode_id, &old_parent_inode);
        if (err != OK) return err;

    } else {
        // create entry in the new directory, save inode
        err = dir_entry_append(mt, &new_parent_inode, new_filename, file_inode_id);
        if (err != OK) return err;
        err = inode_persist(mt, new_parent_inode_id, &new_parent_inode);
        if (err != OK) return err;

        // remove from old, save old inode
        err = dir_entry_delete(mt, &old_parent_inode, old_entry_no);
        if (err != OK) return err;
        err = inode_persist(mt, old_parent_inode_id, &old_parent_inode);
        if (err != OK) return err;
    }

    return OK;
}

static void sfs_dump_debug_info(simple_filesystem *sfs, const char *title) {
    if (sfs == NULL) return;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return;

    if (title != NULL)
        printf("---------------------- %s ----------------------\n", title);
    
    printf("Mounted     [RO:%d]\n", mt->readonly);
    cache_dump_debug_info(mt->cache);
    superblock_dump_debug_info(mt->superblock);
    bitmap_dump_debug_info(mt);
    printf("Root directory\n");
    dir_dump_debug_info(mt, &mt->superblock->root_dir_inode, 1);
    open_dump_debug_info(mt);
    // data->device->dump_debug_info(data->device, "");

    // it would be fun, if we discovered where each block is allocated for and print it.
}

// ------------------------------------------------------------------

simple_filesystem *new_simple_filesystem(mem_allocator *memory, sector_device *device, clock_device *clock) {

    if (sizeof(block_range) != 6) { // written on disk, in the indirect blocks
        printf("sizeof(block_range): %ld\n", sizeof(block_range));
        exit(1);
    }
    if (sizeof(inode) != 64) { // written on disk, must be same size
        printf("sizeof(inode): %ld\n", sizeof(inode));
        exit(1);
    }
    if (sizeof(direntry) != 64) { // written on disk, must be same size
        printf("sizeof(direntry): %ld\n", sizeof(direntry));
        exit(1);
    }
    if (sizeof(superblock) != 512) { // must be able to load in a single 512B sector
        printf("sizeof(superblock): %ld\n", sizeof(superblock));
        exit(1);
    }

    filesys_data *sfs_data = memory->allocate(memory, sizeof(filesys_data));
    sfs_data->memory = memory;
    sfs_data->device = device;
    sfs_data->clock = clock;
    sfs_data->mounted = NULL; // we are not mounted, for now

    simple_filesystem *sfs = memory->allocate(memory, sizeof(simple_filesystem));
    sfs->sfs_data = sfs_data;

    sfs->mkfs = sfs_mkfs;
    sfs->mount = sfs_mount;
    sfs->sync = sfs_sync;
    sfs->unmount = sfs_unmount;

    sfs->open = sfs_open;
    sfs->read = sfs_read;
    sfs->write = sfs_write;
    sfs->close = sfs_close;
    sfs->seek = sfs_seek;
    sfs->tell = sfs_tell;

    sfs->open_dir = sfs_open_dir;
    sfs->read_dir = sfs_read_dir;
    sfs->close_dir = sfs_close_dir;

    sfs->stat = sfs_stat;
    sfs->create = sfs_create;
    sfs->truncate = sfs_truncate;
    sfs->unlink = sfs_unlink;
    sfs->rename = sfs_rename;

    sfs->dump_debug_info = sfs_dump_debug_info;
    return sfs;
}

// ------------------------------------------------------------------
