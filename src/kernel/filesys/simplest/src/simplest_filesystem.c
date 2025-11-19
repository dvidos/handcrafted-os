#include "../dependencies/returns.h"
#include "../simplest_filesystem.h"
#include <stdlib.h>
#include <string.h>
#include "internal.h"



// ------------------------------------------------------------------

static int populate_superblock(uint32_t sector_size, uint32_t sector_count, uint32_t desired_block_size, superblock *superblock) {
    superblock->magic[0] = 'S';
    superblock->magic[1] = 'F';
    superblock->magic[2] = 'S';
    superblock->magic[3] = '1';

    uint32_t blk_size;
    uint64_t capacity = (uint64_t)sector_size * (uint64_t)sector_count;

    if (desired_block_size > 0) {
        // must be a multiple of sector size.
        if (desired_block_size % sector_size != 0)
            return ERR_NOT_SUPPORTED;
        if (desired_block_size < sector_size || desired_block_size >= 4*KB)
            return ERR_NOT_SUPPORTED;
        
        blk_size = desired_block_size;
    } else {
        // auto determine
        /*
            Disk Capacity                     2MB     8MB     32MB    1GB
            -------------------------------------------------------------
            Block size (bytes)                512      1K      2K      4K
            Total blocks                     4096    8192   16384  262144
            Blocks for used/free bitmaps        1       1       1       8
            Availability bitmap size (bytes)  512      1K      2K     32K
        */
        if (capacity <=  2*MB)      blk_size = 512;
        else if (capacity <=  8*MB) blk_size = 1*KB;
        else if (capacity <= 32*MB) blk_size = 2*KB;
        else blk_size = 4*KB;
        if (blk_size < sector_size)
            blk_size = sector_size;
    }

    // block size must be a multiple of sector size, for performance
    if (blk_size % sector_size != 0)
        return ERR_NOT_SUPPORTED;
    
    superblock->sector_size = sector_size;
    superblock->block_size_in_bytes = blk_size;
    superblock->sectors_per_block = blk_size / sector_size;
    superblock->blocks_in_device = (uint32_t)(capacity / blk_size);

    // blocks needed for bitmaps to track used/free blocks
    uint32_t bitmap_bytes = ceiling_division(superblock->blocks_in_device, 8);
    superblock->block_allocation_bitmap_blocks_count = ceiling_division(bitmap_bytes, superblock->block_size_in_bytes);
    superblock->block_allocation_bitmap_first_block = 1;

    return OK;
}

static int read_block_range_low_level(sector_device *dev, uint32_t sector_size, uint32_t sectors_per_block, uint32_t first_block, uint32_t block_count, void *buffer) {
    // very simple to allow us loading the superblock on mounting
    if (dev == NULL)
        return ERR_NOT_SUPPORTED;

    int sector_number = first_block * sectors_per_block;
    int sector_count = block_count * sectors_per_block;
    void *target = buffer;
    while (sector_count-- > 0) {
        int err = dev->read_sector(dev, sector_number, target);
        if (err != OK) return err;
        sector_number += 1;
        target += sector_size;
    }

    return OK;
}

static int read_block_range(filesys_data *data, uint32_t first_block, uint32_t block_count, void *buffer) {
    if (data->mounted == NULL || data->mounted->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (first_block >= sb->blocks_in_device || first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    
    return read_block_range_low_level(
        data->device,
        sb->sector_size,
        sb->sectors_per_block,
        first_block,
        block_count,
        buffer
    );
}

static int write_block_range(filesys_data *data, uint32_t first_block, uint32_t block_count, void *buffer) {
    if (data->mounted == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (first_block >= sb->blocks_in_device || first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int sector_size = sb->sector_size;
    int first_sector = first_block * sb->sectors_per_block;
    int sector_count = block_count * sb->sectors_per_block;
    void *target = buffer;
    for (int i = 0; i < sector_count; i++) {
        int err = data->device->write_sector(data->device, first_sector + i, target);
        if (err != OK) return err;
        target += sector_size;
    }

    return OK;
}

static int copy_block_range(filesys_data *data, uint32_t source_first_block, uint32_t dest_first_block, uint32_t block_count) {
    if (data->mounted == NULL || data->mounted->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->mounted->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->mounted->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (source_first_block >= sb->blocks_in_device || source_first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    if (dest_first_block >= sb->blocks_in_device || dest_first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    if (source_first_block == dest_first_block || block_count == 0)
        return OK;

    int err;
    int source_sector = source_first_block * sb->sectors_per_block;
    int dest_sector = dest_first_block * sb->sectors_per_block;
    int sector_count = block_count * sb->sectors_per_block;
    while (sector_count-- > 0) {
        err = data->device->read_sector(data->device, source_sector, data->mounted->scratch_block_buffer);
        if (err != OK) return err;
        err = data->device->write_sector(data->device, dest_sector, data->mounted->scratch_block_buffer);
        if (err != OK) return err;
        source_sector += 1;
        dest_sector += 1;
    }

    return OK;
}

// ------------------------------------------------------------------

#include "blocks.inc.c"
#include "ranges.inc.c"

// ------------------------------------------------------------------

// given an inode, it resolves the relative block index in the file, into the block_no on the disk
static int find_block_no_from_file_block_index(mounted_data *mt, inode *file_inode, uint32_t block_index_in_file, uint32_t *absolute_block_no) {

    // first, try the inline ranges
    for (int i = 0; i < RANGES_IN_INODE; i++) {
        if (is_range_empty(&file_inode->ranges[i]))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&file_inode->ranges[i], &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if there is no extra ranges block, we cannot find it
    if (file_inode->indirect_ranges_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    
    int ranges_in_block = mt->superblock->block_size_in_bytes / sizeof(block_range);
    block_range range;
    for (int i = 0; i < ranges_in_block; i++) {
        int err = mt->cache->read(mt->cache, file_inode->indirect_ranges_block_no, sizeof(block_range) * i, &range, sizeof(block_range));
        if (err != OK) return err;

        if (is_range_empty(&range))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&range, &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if here, we have exhausted all ranges, relative_block is outside of them all
    // it means we need more ranges, or two-step ranges for bigger files. but... for another day.
    return ERR_OUT_OF_BOUNDS;
}

// try to extend, allocate new, or fail to fallback
static int add_block_to_array_of_ranges(mounted_data *mt, block_range *ranges_array, int ranges_count, int fallback_available, int *use_fallback, uint32_t *new_block_no) {
    int last_used_idx;
    int first_free_idx;
    int err;

    // by default, we won't need to use the fallback.
    *use_fallback = 0;

    find_last_used_and_first_free_range(ranges_array, ranges_count, &last_used_idx, &first_free_idx);
    if (first_free_idx >= 0) {

        // if there is one already use, try to extend it.
        if (last_used_idx >= 0) {
            err = extend_range_by_allocating_block(mt, &ranges_array[last_used_idx], new_block_no);
            // if successful, we are good
            if (err == OK) return OK;
        }

        // there is nothing to extend, or we failed to extend. allocate the free one
        err = initialize_range_by_allocating_block(mt, &ranges_array[first_free_idx], new_block_no);
        if (err != OK) return err;

        return OK;
    }

    // so, no free ranges are available, we may have an indirect block though
    // only if there is one last used, and there is no indirect, there is a reason to try to extend
    if (!fallback_available && last_used_idx >= 0) {
        err = extend_range_by_allocating_block(mt, &ranges_array[last_used_idx], new_block_no);
        // if successful, we are good
        if (err == OK) return OK;
    }

    // there is no fallback, it's a clear failure
    if (!fallback_available) {
        use_fallback = 0;
        return ERR_RESOURCES_EXHAUSTED;
    }
    
    // in all other cases (including failing to extend), we need the indirect block
    *use_fallback = 1;
    return OK;
}

static int add_data_block_to_file(mounted_data *mt, inode *inode, uint32_t *absolute_block_no) {
    int err;
    int use_indirect_block;

    err = add_block_to_array_of_ranges(mt, 
        inode->ranges, RANGES_IN_INODE, 
        inode->indirect_ranges_block_no != 0, &use_indirect_block, 
        absolute_block_no);
    if (err == OK && !use_indirect_block) return OK;
    
    // so either we failed, or we need the indirect block
    if (!use_indirect_block)
        return err;
    
    // load or create the indirect block?
    if (inode->indirect_ranges_block_no == 0) {
        err = mt->bitmap->find_a_free_block(mt->bitmap, &inode->indirect_ranges_block_no);
        if (err != OK) return err;
        mt->bitmap->mark_block_used(mt->bitmap, inode->indirect_ranges_block_no);
        err = mt->cache->wipe(mt->cache, inode->indirect_ranges_block_no);
        if (err != OK) return err;
    } else {
        err = mt->cache->read(mt->cache, 
            inode->indirect_ranges_block_no, 0,
            mt->scratch_block_buffer,
            mt->superblock->block_size_in_bytes);
        if (err != OK) return err;
    }

    err = add_block_to_array_of_ranges(mt, 
        (block_range *)mt->scratch_block_buffer, 
        mt->superblock->block_size_in_bytes / sizeof(block_range), 
        0, // no fallback exists
        &use_indirect_block, // actualy, we don't care
        absolute_block_no);
    if (err != OK) return err;

    // write back the changes
    err = mt->cache->write(mt->cache, 
        inode->indirect_ranges_block_no, 0,
        mt->scratch_block_buffer,
        mt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    // finally! this was a lot!
    return OK;
}

static int read_inode_from_inodes_database(filesys_data *data, int inode_no, inode_in_mem *target) {
    int err;

    // find the block where this inode is stored in
    uint32_t inodes_per_block = data->mounted->superblock->block_size_in_bytes / sizeof(inode);
    uint32_t block_index_in_file = inode_no / inodes_per_block;
    uint32_t disk_block_no = 0;
    err = find_block_no_from_file_block_index(data->mounted, &data->mounted->superblock->inodes_db_inode, block_index_in_file, &disk_block_no);
    if (err != OK) return err;

    // now we know the absolute block, we can read it
    uint32_t offset_in_block = (inode_no % inodes_per_block) * sizeof(inode);
    err = data->mounted->cache->read(data->mounted->cache, disk_block_no, offset_in_block, &target->inode, sizeof(inode));
    if (err != OK) return err;

    target->flags = 0; // not dirty
    target->inode_no = inode_no;
    return OK;
}

static int write_inode_to_inodes_database() {
    return ERR_NOT_IMPLEMENTED;
}

static int insert_inode_to_inodes_database() {
    return ERR_NOT_IMPLEMENTED;
}

static int delete_inode_from_inodes_database() {
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
    err = write_block_range(sfs->sfs_data, 0, 1, data->mounted->superblock);
    if (err != OK) return err;

    // write used blocks bitmap to disk
    err = write_block_range(sfs->sfs_data, 
        data->mounted->superblock->block_allocation_bitmap_first_block, 
        data->mounted->superblock->block_allocation_bitmap_blocks_count, 
        data->mounted->used_blocks_bitmap
    );
    if (err != OK) return err;

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

// ------------------------------------------------------------------

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

    return sfs;
}

// ------------------------------------------------------------------

