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

// ------------------------------------------------------------------

static int resolve_path_to_inode(mounted_data *mt, const char *path, int resolve_parent_dir_only, inode *target, uint32_t *inode_id) {
    char part_name[MAX_FILENAME_LENGTH + 1];
    int err;
    inode curr_dir;
    uint32_t curr_dir_inode_id;
    inode entry_inode;
    uint32_t entry_inode_id;

    // all paths should be absolute
    if (path[0] != '/')
        return ERR_INVALID_ARGUMENT;
    path += 1;

    // path was "/" only.
    if (*path == '\0') {
        if (resolve_parent_dir_only)
            return ERR_INVALID_ARGUMENT;

        memcpy(target, &mt->superblock->root_dir_inode, sizeof(inode));
        *inode_id = ROOT_DIR_INODE_ID;
        return OK;
    }

    // start walking down the directories from root dir
    memcpy(&curr_dir, &mt->superblock->root_dir_inode, sizeof(inode));
    curr_dir_inode_id = ROOT_DIR_INODE_ID;
    while (1) {

        // get first part of the path
        path_get_first_part(path, part_name, sizeof(part_name));
        path += strlen(part_name);
        if (*path == '/')
            path++;
        int finished = (*path == '\0');

        // if we are resolving parent, we don't need to seek the last part
        if (resolve_parent_dir_only && finished) {
            memcpy(target, &curr_dir, sizeof(inode));
            *inode_id = curr_dir_inode_id;
            return OK;
        }

        // look up the chunk in the current directory
        err = dir_entry_find(mt, &curr_dir, part_name, &entry_inode_id, NULL);
        if (err != OK) return err;

        // load so we can return it, or use it.
        err = inode_load(mt, entry_inode_id, &entry_inode);
        if (err != OK) return err;

        if (finished) {
            memcpy(target, &entry_inode, sizeof(inode));
            *inode_id = entry_inode_id;
            return OK;
        }

        // verify we recurse into a directory that we can search in
        if (!entry_inode.is_dir)
            return ERR_WRONG_TYPE;
        memcpy(&curr_dir, &entry_inode, sizeof(inode));
        curr_dir_inode_id = entry_inode_id;
    }

    // should never happen
    return ERR_NOT_FOUND;
}

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

    mark_block_used(mt, 0);
    for (int i = 0; i < sb->blocks_bitmap_blocks_count; i++)
        mark_block_used(mt, sb->blocks_bitmap_first_block + i);

    // we can start writing
    cache_data *c = initialize_cache(data->memory, data->device, sb->block_size_in_bytes);

    err = cached_write(c, 0, 0, sb, sizeof(superblock));
    if (err != OK) return err;

    for (int i = 0; i < sb->blocks_bitmap_blocks_count; i++) {
        err = cached_write(c, sb->blocks_bitmap_first_block + i, 
            0,
            mt->used_blocks_bitmap + (sb->block_size_in_bytes * i), 
            sb->block_size_in_bytes);
        if (err != OK) return err;
    }

    // persist everything and we're done
    err = cache_flush(c);
    if (err != OK) return err;

    return OK;
}

static int sfs_fsck(simple_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
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
    mounted_data *mount = data->memory->allocate(data->memory, sizeof(mounted_data));
    mount->readonly = readonly;
    mount->superblock = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mount->used_blocks_bitmap = data->memory->allocate(data->memory, sb->blocks_bitmap_blocks_count * sb->block_size_in_bytes);
    mount->cache = initialize_cache(data->memory, data->device, sb->block_size_in_bytes);
    mount->generic_block_buffer = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    data->mounted = mount;
    
    // we can release temp sector now
    data->memory->release(data->memory, temp_sector);

    // cache can now read the blocks
    err = cached_read(mount->cache, 0, 0, mount->superblock, sizeof(superblock));
    if (err != OK) return err;

    // load blocks bitmap
    for (int i = 0; i < mount->superblock->blocks_bitmap_blocks_count; i++) {
        err = cached_read(mount->cache,
            mount->superblock->blocks_bitmap_first_block + i, 
            0,
            mount->used_blocks_bitmap + (i * mount->superblock->block_size_in_bytes), 
            mount->superblock->block_size_in_bytes);
        if (err != OK) return err;
    }

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
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        open_inode *node = &mt->open_inodes[i];
        if (node->is_used && node->is_dirty) {
            err = open_inodes_flush_inode(mt, node);
            if (err != OK) return err;
        }
    }
    
    // write superblock to cache
    err = cached_write(mt->cache, 0, 0, mt->superblock, sizeof(superblock));
    if (err != OK) return err;

    // write used blocks bitmap to disk (blocks 1+)
    for (int i = 0; i < mt->superblock->blocks_bitmap_blocks_count; i++) {
        err = cached_write(mt->cache, 
            mt->superblock->blocks_bitmap_first_block + i, 
            0, mt->used_blocks_bitmap + (i * mt->superblock->block_size_in_bytes),
            mt->superblock->block_size_in_bytes
        );
        if (err != OK) return err;
    }

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
    err = resolve_path_to_inode(mt, filename, 0, &inode, &inode_id);
    if (err != OK) return err;
    if (!inode.is_file)
        return ERR_WRONG_TYPE;

    // reuse or create entry in open inodes array
    open_inode *oinode;
    err = open_inodes_register(mt, &inode, inode_id, &oinode);
    if (err != OK) return err;

    // create entry the open files array
    open_handle *handle;
    err = open_handles_register(mt, oinode, &handle);
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
    return OK;
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
    return OK;
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
    err = resolve_path_to_inode(mt, path, 0, &inode, &inode_id);
    if (err != OK) return err;
    if (!inode.is_dir)
        return ERR_WRONG_TYPE;

    // reuse or create entry in open inodes array
    open_inode *oinode;
    err = open_inodes_register(mt, &inode, inode_id, &oinode);
    if (err != OK) return err;

    // create entry the open files array
    open_handle *handle;
    err = open_handles_register(mt, oinode, &handle);
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

static int sfs_create(simple_filesystem *sfs, char *path, int is_dir) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    int err;
    
    // see if path resolves to inode
    inode parent_inode;
    uint32_t parent_inode_id;
    err = resolve_path_to_inode(mt, path, 1, &parent_inode, &parent_inode_id);
    if (err != OK) return err;

    // find desired name
    const char *desired_name = path_get_last_part(path);
    if (strlen(desired_name) == 0 || strcmp(desired_name, ".") == 0 || strcmp(desired_name, "..") == 0)
        return ERR_INVALID_ARGUMENT;

    // see if name exists in the directory
    uint32_t new_inode_id;
    err = dir_entry_find(mt, &parent_inode, desired_name, &new_inode_id, NULL);
    if (err == OK) return ERR_CONFLICT;

    // now we should be able to create a file / dir
    inode new_inode;
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

static int sfs_unlink(simple_filesystem *sfs, char *path, int options) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    int err;
    
    // see if path resolves to inode
    inode parent_inode;
    uint32_t parent_inode_id;
    err = resolve_path_to_inode(mt, path, 1, &parent_inode, &parent_inode_id);
    if (err != OK) return err;

    // see if name exists in the directory
    uint32_t new_inode_id;
    uint32_t direntry_rec_no;
    err = dir_entry_find(mt, &parent_inode, path_get_last_part(path), &new_inode_id, &direntry_rec_no);
    if (err != OK) return err;

    // TODO: since we don't support multiple links to inodes, we need to remove both the inode, and free all the data blocks...

    // now we can unlink this (e.g. nullify dir entry)
    direntry entry;
    memset(&entry, 0, sizeof(direntry));
    dir_entry_update(mt, &parent_inode, direntry_rec_no, "", 0);

    // we must persist the parent inode changes (but not much changed...)
    err = inode_persist(mt, parent_inode_id, &parent_inode);
    if (err != OK) return err;

    return OK;
}

static int sfs_rename(simple_filesystem *sfs, char *oldpath, char *newpath) {
    // we need an unlink that will not remove the inode, nor file blocks,
    // then link from the new directory.
    // actually, try creating the new link first, then remove the old one
    
    // actually, this points to the important internal functions of a file system:
    // - inode manipulation (+range manipulation for finding, extending, releasing)
    // - blocks manipulation (allocation, freeing)
    // - directory entries manipulation (resolve, find, add, remove, modify, skip/reuse empty slots)
    // - cached read/write operations
    // notes:
    // - at a later time, instead of copying inodes to work on them, open then in the open_inodes array, so we don't need to save them back.
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

simple_filesystem *new_simple_filesystem(mem_allocator *memory, sector_device *device) {

    if (sizeof(inode) != 64) // written on disk, must be same size
        return NULL;
    if (sizeof(direntry) != 64) // written on disk, must be same size
        return NULL;
    if (sizeof(superblock) != 512) // must be able to load in a single 512B sector
        return NULL;

    filesys_data *sfs_data = memory->allocate(memory, sizeof(filesys_data));
    sfs_data->memory = memory;
    sfs_data->device = device;
    sfs_data->mounted = NULL; // we are not mounted, for now

    simple_filesystem *sfs = memory->allocate(memory, sizeof(simple_filesystem));
    sfs->sfs_data = sfs_data;

    sfs->mkfs = sfs_mkfs;
    sfs->fsck = sfs_fsck;

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
    sfs->create = sfs_create;
    sfs->unlink = sfs_unlink;
    sfs->rename = sfs_rename;

    sfs->dump_debug_info = sfs_dump_debug_info;
    return sfs;
}

// ------------------------------------------------------------------

