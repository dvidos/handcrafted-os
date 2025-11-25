#include "../dependencies/returns.h"
#include "../simple_filesystem.h"
#include <stdlib.h>
#include <string.h>
#include "internal.h"

// ------------------------------------------------------------------

#include "superblock.inc.c"
#include "bitmap.inc.c"
#include "cache.inc.c"
#include "ranges.inc.c"
#include "block_ops.inc.c"
#include "inodes.inc.c"
#include "open.inc.c"

// ------------------------------------------------------------------

static int resolve_path_to_inode(mounted_data *mt, const char *path, inode *target, uint32_t *inode_rec_no) {

    if (strcmp(path, "/") == 0) {
        memcpy(target, &mt->superblock->root_dir_inode, sizeof(inode));
        *inode_rec_no = ROOT_DIR_INODE_REC_NO;
        return OK;
    }

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

    return ERR_OUT_OF_BOUNDS;
}

// ------------------------------------------------------------------

static int sfs_mkfs(simple_filesystem *sfs, char *volume_label, uint32_t desired_block_size) {
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

    mounted_data *mt = data->memory->allocate(data->memory, sizeof(mounted_data));
    mt->superblock = sb;
    mt->used_blocks_bitmap = data->memory->allocate(data->memory, ceiling_division(sb->blocks_in_device, 8));

    // reserved blocks
    mark_block_used(mt, 0);
    for (int i = 0; i < sb->blocks_bitmap_blocks_count; i++)
        mark_block_used(mt, sb->blocks_bitmap_first_block + i);

    // now we know the block size. let's save things.
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
    int recognized = (memcmp(sb->magic, "SFS1", 4) == 0);
    if (!recognized) {
        data->memory->release(data->memory, temp_sector);
        return ERR_NOT_RECOGNIZED;
    }

    // ok, we are safe to load, prepare memory
    mounted_data *mount = data->memory->allocate(data->memory, sizeof(mounted_data));
    mount->readonly = readonly;
    mount->superblock = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    mount->used_blocks_bitmap = data->memory->allocate(data->memory, ceiling_division(sb->blocks_in_device, 8));
    mount->cache = initialize_cache(data->memory, data->device, sb->block_size_in_bytes);
    mount->generic_block_buffer = data->memory->allocate(data->memory, sb->block_size_in_bytes);
    data->mounted = mount;
    
    // read superblock, low level, since we are unmounted
    err = read_block_range_low_level(
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
        data->mounted->superblock->blocks_bitmap_first_block, 
        data->mounted->superblock->blocks_bitmap_blocks_count, 
        data->mounted->used_blocks_bitmap
    );
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


    // before saving the superblock, we should write all the open inodes back to the inodes db
    // TODO: do this.
    
    // write superblock
    err = cached_write(mt->cache, 0, 0, mt->superblock, sizeof(superblock));
    if (err != OK) return err;

    // write used blocks bitmap to disk
    for (int i = 0; i < mt->superblock->blocks_bitmap_blocks_count; i++) {
        err = cached_write(mt->cache, 
            mt->superblock->blocks_bitmap_first_block + i, 
            0, mt->used_blocks_bitmap + (i * mt->superblock->block_size_in_bytes),
            mt->superblock->block_size_in_bytes
        );
        if (err != OK) return err;
    }

    // flush all caches
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

    inode inode;
    uint32_t inode_rec_no;
    int err = resolve_path_to_inode(mt, filename, &inode, &inode_rec_no);
    if (err != OK) return err;

    
    // need to walk the directories to find the inode
    // then, need to load the inode into the open_files -> inode_in_memory
    // then, create a handle to point to this inode_in_memory
    // mount should have an array of open inodes_in_mem?
    

    return ERR_NOT_IMPLEMENTED;
}

static int sfs_read(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_write(simple_filesystem *sfs, sfs_handle *h, void *buffer, uint32_t size) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_close(simple_filesystem *sfs, sfs_handle *h) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    superblock *sb = data->mounted->superblock;
    if (data == NULL || sb == NULL)
        return ERR_NOT_SUPPORTED;
        
    // we need to flush out all the open cache slots of this file
    // we need to save inode info to disk
   return ERR_NOT_IMPLEMENTED;
}

static int sfs_seek(simple_filesystem *sfs, sfs_handle *h, int offset, int origin) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_tell(simple_filesystem *sfs, sfs_handle *h) {
    return ERR_NOT_IMPLEMENTED;
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
    uint32_t inode_rec_no;
    err = resolve_path_to_inode(mt, path, &inode, &inode_rec_no);
    if (err != OK) return err;
    if (!inode.is_dir)
        return ERR_WRONG_TYPE;

    // reuse or create entry in open inodes array
    open_inode *oinode;
    err = open_inodes_allocate(mt, &inode, inode_rec_no, &oinode);
    if (err != OK) return err;

    // create entry the open files array
    open_file *ofile;
    err = open_files_allocate(mt, oinode, &ofile);
    if (err != OK) return err;

    *handle_ptr = (sfs_handle *)ofile;
    return OK;
}

static int sfs_read_dir(simple_filesystem *sfs, sfs_handle *h, sfs_dir_entry *entry) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;


    return ERR_NOT_IMPLEMENTED;
}

static int sfs_close_dir(simple_filesystem *sfs, sfs_handle *h) {
    if (sfs == NULL) return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    if (data == NULL) return ERR_NOT_SUPPORTED;
    mounted_data *mt = data->mounted;
    if (mt == NULL) return ERR_NOT_SUPPORTED;
    
    open_file *ofile = (open_file *)h;
    if (ofile == NULL || ofile->inode == NULL || !ofile->is_used)
        return ERR_INVALID_ARGUMENT;

    int err = open_files_release(mt, ofile);
    if (err != OK) return err;

    return OK;
}

static int sfs_create(simple_filesystem *sfs, char *path, int options) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL) return ERR_NOT_IMPLEMENTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    
    // resolve to parent path
    // check it is a directory
    // open it, go over names, ensure it does not exist
    // append to directory file
    // create inode, flags/options tell us if it's a directory or file, or otherwise
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_unlink(simple_filesystem *sfs, char *path, int options) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    superblock *sb = data->mounted->superblock;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    if (sb == NULL) return ERR_NOT_IMPLEMENTED;
    
    // resolve to parent path
    // check if directory
    // open it, go over entries, find name
    // if it's a directory, ensure it's not empty
    // else remove it (null it out)
    // clear inode as well
    // release the blocks the inode used to own
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_rename(simple_filesystem *sfs, char *oldpath, char *newpath) {
    if (sfs == NULL || sfs->sfs_data == NULL || ((filesys_data *)sfs->sfs_data)->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    filesys_data *data = sfs->sfs_data;
    superblock *sb = data->mounted->superblock;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    if (sb == NULL) return ERR_NOT_IMPLEMENTED;
    
    // resolve to both parent paths
    // on old, enumerate, ensure you find entry
    // on new, enumerate, ensure it does not exist
    // null out entry in old directory
    // append new entry in new directory
    return ERR_NOT_IMPLEMENTED;
}

// ------------------------------------------------------------------

simple_filesystem *new_simple_filesystem(mem_allocator *memory, sector_device *device) {

    if (sizeof(inode) != 64) // written on disk, must be same size
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

    return sfs;
}

// ------------------------------------------------------------------

