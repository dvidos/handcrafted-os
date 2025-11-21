#include "../dependencies/returns.h"
#include "../simplest_filesystem.h"
#include <stdlib.h>
#include <string.h>
#include "internal.h"

// ------------------------------------------------------------------

#include "superblock.inc.c"
#include "bitmap.inc.c"
#include "ranges.inc.c"
#include "block_ops.inc.c"
#include "inodes_db.inc.c"

// ------------------------------------------------------------------

static int resolve_path_to_inode(mounted_data *mt, const char *path, inode **inode) {
    // int index = 0;
    // char *buffer;

    // // start from root dir, all the way to find the file.
    // if (path == NULL || path[index] != '/')
    //     return ERR_NOT_SUPPORTED; // all paths must be absolute

    // index++; // skip initial slash
    // if (path[index] == 0) {
    //     inode = mt->superblock->root_dir_inode;
    //     return OK;
    // }
    
    // // so we do have a path
    // char *name[64];
    // int inode_offset;
    // inode *curr_dir = mt->superblock->root_dir_inode;
    // while (true) {
    //     char *slash = strchr(path + index, '/');
    //     if (slash == NULL)
    //         strncpy(name, path + index); // only one remaining
    //     else
    //         strncpy(name, path + index, slash - (path + index)); // found a slash
        
    //     // open this, find entry, go on.
    //     find_named_entry_in_directory(curr_dir, name, &inode_offset);

    //     // found entry, see if this is the last one.
    //     if (strcmp(path + index, "") == 0 || strcmp(path + index, "/") == 0)
    //         break; // got it!
        
    //     // else open this directory
    //     // check that it is a directory
    //     curr_dir = read_inode_from_inodes_database(data, inode_offset)
    // }
    return ERR_NOT_IMPLEMENTED;
}

// ------------------------------------------------------------------

static int sfs_mkfs(simplest_filesystem *sfs, char *volume_label, uint32_t desired_block_size) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted != NULL)
        return ERR_NOT_SUPPORTED;

    // prepare a temp superblock, then save it.
    superblock *sb = data->memory->allocate(data->memory, sizeof(superblock));
    int err = populate_superblock(
        data->device->get_sector_size(data->device),
        data->device->get_sector_count(data->device),
        desired_block_size,
        sb
    );
    if (err != OK) return err;

    // prepare block bitmap, mark SB and bitmap blocks as used
    block_bitmap *bb = new_bitmap(data->memory, sb->blocks_in_device, sb->block_allocation_bitmap_blocks_count, sb->block_size_in_bytes);
    bb->mark_block_used(bb, 0); // for superblock
    for (int i = 0; i < sb->block_allocation_bitmap_blocks_count; i++)
        bb->mark_block_used(bb, sb->block_allocation_bitmap_first_block + i);

    // now we know the block size. let's save things.
    cache_layer *cl = new_cache_layer(data->memory, data->device, sb->block_size_in_bytes, 32);

    cl->write(cl, 0, 0, sb, sizeof(superblock));
    if (err != OK) return err;

    for (int i = 0; i < bb->data->bitmap_size_in_blocks; i++) {
        err = cl->write(cl, sb->block_allocation_bitmap_first_block + i, 
            0,
            bb->data->buffer + (sb->block_size_in_bytes * i), 
            sb->block_size_in_bytes);
        if (err != OK) return err;
    }

    err = cl->flush(cl);
    if (err != OK) return err;

    return OK;
}

static int sfs_fsck(simplest_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_mount(simplest_filesystem *sfs, int readonly) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted != NULL)
        return ERR_NOT_SUPPORTED;

    // first check the first sector to see if we can mount it.
    // hence why the superblock must be at most 512 bytes size.
    uint8_t *temp_sector = data->memory->allocate(data->memory, data->device->get_sector_size(data->device));
    data->device->read_sector(data->device, 0, temp_sector);
    superblock *sb = (superblock *)temp_sector;
    int recognized = (memcmp(sb->magic, "SFS1", 4) == 0);
    if (!recognized) {
        data->memory->release(data->memory, temp_sector);
        return ERR_NOT_RECOGNIZED;
    }

    // ok, we are safe to load, prepare memory
    mounted_data *mount = data->memory->allocate(data->memory, sizeof(mounted_data));
    mount->readonly = readonly;
    mount->superblock = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mount->used_blocks_bitmap = data->memory->allocate(data->memory, sb->block_allocation_bitmap_blocks_count * sb->block_size_in_bytes);
    mount->scratch_block_buffer = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mount->opened_inodes_in_mem = data->memory->allocate(data->memory, sizeof(inode_in_mem) * MAX_OPEN_FILES);
    mount->cache = new_cache_layer(data->memory, data->device, sb->block_size_in_bytes, 64);
    mount->bitmap = new_bitmap(data->memory, sb->blocks_in_device, sb->block_allocation_bitmap_blocks_count, sb->block_size_in_bytes);
    data->mounted = mount;
    
    // read superblock, low level, since we are unmounted
    int err = read_block_range_low_level(
        data->device,
        sb->sector_size,
        sb->sectors_per_block,
        0, // first block
        1, // block count
        data->mounted->superblock
    );

    // make sure to free early, before anything else
    data->memory->release(data->memory, temp_sector);
    if (err != OK) return err;

    // read used blocks bitmap from disk
    err = read_block_range_low_level(
        data->device, 
        data->mounted->superblock->sector_size,
        data->mounted->superblock->sectors_per_block,
        data->mounted->superblock->block_allocation_bitmap_first_block, 
        data->mounted->superblock->block_allocation_bitmap_blocks_count, 
        data->mounted->used_blocks_bitmap
    );
    if (err != OK) return err;

    return OK;
}

static int sfs_sync(simplest_filesystem *sfs) {
    int err;

    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;

    // write superblock
    err = write_block(sfs->sfs_data, 0, data->mounted->superblock);
    if (err != OK) return err;

    // write used blocks bitmap to disk
    for (int i = 0; i < data->mounted->superblock->block_allocation_bitmap_blocks_count; i++) {
        err = write_block(sfs->sfs_data, 
            data->mounted->superblock->block_allocation_bitmap_first_block + i, 
            data->mounted->used_blocks_bitmap + (i * data->mounted->superblock->block_size_in_bytes)
        );
        if (err != OK) return err;
    }

    // write the inodes_database and root_directory inodes and handles

    // write all the inodes opened in memory

    // all caches flush
    data->mounted->cache->flush(data->mounted->cache);

    return OK;
}

static int sfs_unmount(simplest_filesystem *sfs) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;

    if (!data->mounted->readonly) {
        int err = sfs_sync(sfs);
        if (err != OK) return err;
    }

    data->mounted->cache->release_memory(data->mounted->cache);
    data->mounted->bitmap->release_memory(data->mounted->bitmap);

    data->memory->release(data->memory, data->mounted->superblock);
    data->memory->release(data->memory, data->mounted->used_blocks_bitmap);
    data->memory->release(data->memory, data->mounted->scratch_block_buffer);
    data->memory->release(data->memory, data->mounted->opened_inodes_in_mem);
    data->memory->release(data->memory, data->mounted);
    data->mounted = NULL;

    return OK;
}

static sfs_handle *sfs_open(simplest_filesystem *sfs, char *filename, int options) {
    // need to walk the directories to find the inode
    // then, need to load the inode into the open_files -> inode_in_memory
    // then, create a handle to point to this inode_in_memory
    return NULL;
}

static int sfs_read(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer) {
    if (sfs == NULL || sfs->sfs_data == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->mounted->superblock;

    uint32_t file_block_index = h->file_position / sb->block_size_in_bytes;
    uint32_t offset_in_block  = h->file_position % sb->block_size_in_bytes;
    int bytes_read = 0;

    while (size > 0) {
        if (h->file_position >= h->inode_in_mem->inode.file_size)
            break;
        
        uint32_t block_on_disk = 0;
        int err = find_block_no_from_file_block_index(data->mounted, &h->inode_in_mem->inode, file_block_index, &block_on_disk);
        if (err != OK) return err;

        // read within the file, within the block
        uint32_t chunk_size = size;
        chunk_size = at_most(chunk_size, sb->block_size_in_bytes - offset_in_block);
        chunk_size = at_most(chunk_size, h->inode_in_mem->inode.file_size - h->file_position);

        // read this chunk from this block
        err = data->mounted->cache->read(data->mounted->cache, block_on_disk, offset_in_block, buffer, chunk_size);
        if (err != OK) return err;

        // maybe we blead to subsequent block(s)
        size -= chunk_size;
        buffer += chunk_size;
        h->file_position += chunk_size;
        file_block_index += 1;
        offset_in_block = 0;
        bytes_read += chunk_size;
    }

    return bytes_read;
}

static int sfs_write(simplest_filesystem *sfs, sfs_handle *h, uint32_t size, void *buffer) {
    int err;

    if (sfs == NULL || sfs->sfs_data == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->mounted->superblock;

    // we can write at most at end of file.
    if (h->file_position >= h->inode_in_mem->inode.file_size)
        h->file_position = h->inode_in_mem->inode.file_size;

    uint32_t file_block_index = h->file_position / sb->block_size_in_bytes;
    uint32_t offset_in_block  = h->file_position % sb->block_size_in_bytes;
    int bytes_written = 0;

    // start writing chunks, extend file as needed
    while (size > 0) {
        uint32_t block_on_disk = 0;

        // do we need to extend the file?
        // TODO: we may have more allocated than just the file size (e.g . file_size = 123, block_size = 4096)
        if (h->file_position >= h->inode_in_mem->inode.file_size) {
            err = add_data_block_to_file(data->mounted, &h->inode_in_mem->inode, &block_on_disk);
            if (err != OK) return err;
        } else {
            err = find_block_no_from_file_block_index(data->mounted, &h->inode_in_mem->inode, file_block_index, &block_on_disk);
            if (err != OK) return err;
        }

        // write within the block only
        uint32_t chunk_size = at_most(size, sb->block_size_in_bytes - offset_in_block);

        // write this chunk from this block
        err = data->mounted->cache->write(data->mounted->cache, block_on_disk, offset_in_block, buffer, chunk_size);
        if (err != OK) return err;

        // we may have subsequent block(s)
        size -= chunk_size;
        buffer += chunk_size;
        h->file_position += chunk_size;
        file_block_index += 1;
        offset_in_block = 0;
        bytes_written += chunk_size;
    }

    return bytes_written;
}

static int sfs_close(simplest_filesystem *sfs, sfs_handle *h) {
    // we need to flush out all the open cache slots of this file
    // we need to save inode info to disk
   return ERR_NOT_IMPLEMENTED;
}

static int sfs_seek(simplest_filesystem *sfs, sfs_handle *h, int offset, int origin) {
    if (origin == 0) {  // from start

        if (offset < 0)
            offset = 0;
        else if (offset > h->inode_in_mem->inode.file_size)
            offset = h->inode_in_mem->inode.file_size;

        h->file_position = offset;

    } else if (origin == 1) {  // relative

        if (offset < h->file_position * (-1))
            offset = h->file_position * (-1);
        else if (offset > (h->inode_in_mem->inode.file_size - h->file_position))
            offset = (h->inode_in_mem->inode.file_size - h->file_position);

        h->file_position = h->file_position + offset;

    } else if (origin == 2) {  // from end
        
        if (offset > 0)
            offset = 0;
        else if (offset < h->inode_in_mem->inode.file_size * (-1))
            offset = h->inode_in_mem->inode.file_size * (-1);

        h->file_position = h->inode_in_mem->inode.file_size + origin;

    }

    return OK;
}

static int sfs_tell(simplest_filesystem *sfs, sfs_handle *h) {
    return (int)h->file_position;
}

static sfs_handle *sfs_open_dir(simplest_filesystem *sfs, char *path) {
    return NULL;
}

static int sfs_read_dir(simplest_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_close_dir(simplest_filesystem *sfs, sfs_handle *h) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_create(simplest_filesystem *sfs, char *path, int options) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_unlink(simplest_filesystem *sfs, char *path, int options) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_rename(simplest_filesystem *sfs, char *oldpath, char *newpath) {
    return ERR_NOT_IMPLEMENTED;
}

// ------------------------------------------------------------------

simplest_filesystem *new_simplest_filesystem(mem_allocator *memory, sector_device *device) {

    if (sizeof(inode) != 64) // written on disk, must be same size
        return NULL;
    if (sizeof(superblock) != 512) // must be able to load in a single 512B sector
        return NULL;

    filesys_data *sfs_data = memory->allocate(memory, sizeof(filesys_data));
    sfs_data->memory = memory;
    sfs_data->device = device;
    sfs_data->mounted = NULL; // we are not mounted, for now

    simplest_filesystem *sfs = memory->allocate(memory, sizeof(simplest_filesystem));
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

    return sfs;
}

// ------------------------------------------------------------------

