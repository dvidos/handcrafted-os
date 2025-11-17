#include "../dependencies/returns.h"
#include "../simplest_filesystem.h"
#include <stdlib.h>
#include <string.h>
#include "internal.h"


/*
    Simplest file system, as a fun exercise.
    - reading/writing in blocks. Each block can be 512, 1K, 2K or 4K.
    - first block (block 0) hosts superblock.
    - subsequent blocks have bitmap for which blocks are available and which are not.
    - subsequent block starts the inode table?
    - subsequent block starts the root directory?

    File format
    - file contents are one or more blocks.
    - file metadata exists in inode structure
    
    Directory format
    - directory entries are stored in a file, like all others.
    - entries consist of name and inode block-or-number-or-something

    Inodes
    - structures that describe files.
    - size, type, perms, owners, dates, etc
    - tracks content in small array of ranges, pairs of first_block & blocks_count
    - if internal array exhausted, one block can store many ranges (e.g. 64 ranges in a 512B block, or 512 ranges in a 4K block)
    - they are stored inside a big file. the inode number is the record number in that file.

    Used / free space
    - tracked in a large bitmap of fixed size, one bit per block 1=used, 0=free

    Opening files:
    - when opening files we need to have:
        - in memory copy of the inode, along with a "is-dirty" flag, maybe open handles count (cached in filesys)
        - in memory file handle with pointer to file location, open flags, in-mem-inode reference (cached in apps)
*/


#define KB   (1024)
#define MB   (1024*KB)
#define GB   (1024*MB)

#define MAX_OPEN_FILES    100   // later we can do this dynamic
#define RANGES_IN_INODE     6
#define ceiling_division(x, y)    (((x) + ((y)-1)) / (y))
#define at_most(a, b)             ((b) < (a) ? (b) : (a))
#define in_range(value, low, hi)             ((value) < (low) ? (low) : ((value) > (hi) ? (hi) : (value)))

// -------------------------------------------------------

typedef struct filesys_data filesys_data;
typedef struct runtime_data runtime_data;
typedef struct superblock superblock;
typedef struct block_range block_range;
typedef struct inode inode;
typedef struct inode_in_mem inode_in_mem;

struct block_range { // target size: 8
    uint32_t first_block_no;
    uint32_t blocks_count;
};

struct inode { // target size: 64
    uint8_t flags; // 0=unused, 1=file, 2=dir, etc.
    // maybe permissions (e.g. xrwxrwxrw), and owner user/group
    // maybe creation and/or modification date, uint32, secondsfrom epoch
    uint8_t padding[7];
    uint32_t file_size; // 32 bits mean max 4GB file size.
    block_range ranges[RANGES_IN_INODE]; // size: 48
    // if internal ranges are not enough, this block is full or ranges
    uint32_t indirect_ranges_block_no;
};

struct sfs_handle { // an open file, per app data
    inode_in_mem *inode_in_mem;
    uint32_t file_position;
    // buffers could be much better
    uint32_t curr_data_block_num;
    uint8_t *curr_data_block_buffer;
    uint8_t curr_data_block_dirty;
};

struct filesys_data {
    mem_allocator *memory;
    sector_device *device;
    runtime_data *runtime; // if non-null, the filesystem is mounted
};

struct runtime_data {
    int readonly;
    superblock *superblock;
    cache_layer *cache;

    uint8_t *used_blocks_bitmap; // bitmap of used block, mirrored in memory
    uint8_t *scratch_block_buffer;
    inode_in_mem *opened_inodes_in_mem; // array of structs in place
};

struct superblock { // must be up to 512 bytes, in order to read from unknown device
    char magic[4];  // e.g. "SFS1" version can be in here
    uint32_t sector_size;          // typically 512 to 4K, device driven
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t used_blocks_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t used_blocks_bitmap_blocks_count;       // typically 1 through 16 blocks

    uint8_t padding[512 - 4 - (sizeof(uint32_t) * RANGES_IN_INODE) - (sizeof(inode) * 2)];

    // these two 2*64=128 bytes, still 1/4 of a block...
    inode inodes_db_inode; // file with inodes. inode_no is the record number, zero based.
    inode root_dir_inode;  // file with the entries for root directory. 
};

// ------------------------------------------------------------------

struct inode_in_mem {
    // this is needed to avoid inode operations on disk directly.
    uint32_t flags; // is used, is dirty etc
    uint32_t inode_no;
    inode inode; // mirroring what is (or must be) on disk
};

// ------------------------------------------------------------------

static int populate_superblock(uint32_t sector_size, uint32_t sector_count, superblock *superblock) {
    superblock->magic[0] = 'S';
    superblock->magic[1] = 'F';
    superblock->magic[2] = 'S';
    superblock->magic[3] = '1';

    uint64_t capacity = (uint64_t)sector_size * (uint64_t)sector_count;
    uint32_t blk_size;

    /*
        Disk Capacity                     2MB     8MB     32MB    1GB
        -------------------------------------------------------------
        Block size (bytes)                512      1K      2K      4K
        Total blocks                     4096    8192   16384  262144
        Blocks for used/free bitmaps        1       1       1       8
        Availability bitmap size (bytes)  512      1K      2K     32K
    */

    if (capacity <=  2*MB)
        blk_size = 512;
    else if (capacity <=  8*MB)
        blk_size = 1*KB;
    else if (capacity <= 32*MB)
        blk_size = 2*KB;
    else
        blk_size = 4*KB;

    if (blk_size < sector_size)
        blk_size = sector_size;
    if (blk_size % sector_size != 0)
        return ERR_NOT_SUPPORTED;
    
    superblock->sector_size = sector_size;
    superblock->block_size_in_bytes = blk_size;
    superblock->sectors_per_block = blk_size / sector_size;
    superblock->blocks_in_device = (uint32_t)(capacity / blk_size);

    // blocks needed for bitmaps to track used/free blocks
    uint32_t bitmap_bytes = ceiling_division(superblock->blocks_in_device, 8);
    superblock->used_blocks_bitmap_blocks_count = ceiling_division(bitmap_bytes, superblock->block_size_in_bytes);
    superblock->used_blocks_bitmap_first_block = 1;

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
    if (data->runtime == NULL || data->runtime->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->runtime->superblock;
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
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;
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
    if (data->runtime == NULL || data->runtime->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;
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
        err = data->device->read_sector(data->device, source_sector, data->runtime->scratch_block_buffer);
        if (err != OK) return err;
        err = data->device->write_sector(data->device, dest_sector, data->runtime->scratch_block_buffer);
        if (err != OK) return err;
        source_sector += 1;
        dest_sector += 1;
    }

    return OK;
}

// ------------------------------------------------------------------

static inline int is_block_used(runtime_data *rt, uint32_t block_no) {
    register int byte_no = (block_no / 8);
    register uint8_t mask = 1 << (block_no % 8);
    return (rt->used_blocks_bitmap[byte_no] & mask) != 0;
}

static inline int is_block_free(runtime_data *rt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    return (rt->used_blocks_bitmap[byte_no] & mask) == 0;
}

static inline void mark_block_used(runtime_data *rt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    rt->used_blocks_bitmap[byte_no] |= mask;
}

static inline void mark_block_free(runtime_data *rt, uint32_t block_no) {
    register int byte_no = block_no / 8;
    register uint8_t mask = 1 << (block_no % 8);
    rt->used_blocks_bitmap[byte_no] &= ~mask;
}

static int find_any_free_block(runtime_data *rt, uint32_t *block_no) {
    int bytes_in_bitmap = ceiling_division(rt->superblock->blocks_in_device, 8);
    int byte_index = 0;
    int bit_index = 0;

    // first find a non-filled byte
    for (byte_index = 0; byte_index < bytes_in_bitmap; byte_index++) {
        if (rt->used_blocks_bitmap[byte_index] != 0xFF)
            break;
    }

    // maybe all blocks are used, no more available space
    if (byte_index >= bytes_in_bitmap)
        return ERR_RESOURCES_EXHAUSTED; 

    // find the free bit in this byte
    uint8_t byte_value = rt->used_blocks_bitmap[byte_index];
    for (bit_index = 0; bit_index < 8; bit_index++) {
        if ((byte_value & (1 << bit_index)) == 0)
            break;
    }

    // should not happen, but...
    if (bit_index >= 8)
        return ERR_RESOURCES_EXHAUSTED;

    *block_no = (byte_index * 8) + bit_index;
    return OK;
}

// ------------------------------------------------------------------

static inline int is_range_used(block_range *range) {
    return range->first_block_no != 0;
}

static inline uint32_t range_last_block_no(block_range *range) {
    return range->first_block_no + range->blocks_count - 1;
}

static inline int check_or_consume_blocks_in_range(block_range *range, uint32_t *relative_block_no, uint32_t *absolute_block_no) {
    if (*relative_block_no < range->blocks_count) {
        // we are within range!
        *absolute_block_no = range->first_block_no + *relative_block_no;
        return 1;
    } else {
        // we are not within range, consume this one
        *relative_block_no -= range->blocks_count;
        return 0;
    }
}

static inline void find_last_used_and_first_free_range(block_range *ranges_array, int ranges_count, int *last_used_idx, int *first_free_idx) {
    if (!is_range_used(&ranges_array[0])) {
        *last_used_idx = -1;
        *first_free_idx = 0;
    } else if (is_range_used(&ranges_array[ranges_count - 1])) {
        *last_used_idx = ranges_count - 1;
        *first_free_idx = -1;
    } else {
        for (int i = 1; i < ranges_count; i++) {
            if (is_range_used(&ranges_array[i - 1]) && !is_range_used(&ranges_array[i])) {
                *last_used_idx = i - 1;
                *first_free_idx = i;
                return;
            }
        }

        // should not happen, but...
        *last_used_idx = -1;
        *first_free_idx = -1;
    }
}

static inline int initialize_range_by_allocating_block(runtime_data *rt, block_range *range, uint32_t *block_no) {
    uint32_t new_block_no = 0;
    int err = find_any_free_block(rt, &new_block_no);
    if (err != OK) return err;

    mark_block_used(rt, new_block_no);
    rt->cache->wipe(rt->cache, new_block_no);
    range->first_block_no = new_block_no;
    range->blocks_count = 1;
    *block_no = new_block_no;
    return OK;
}

static inline int extend_range_by_allocating_block(runtime_data *rt, block_range *range, uint32_t *block_no) {
    uint32_t next_block_no = range_last_block_no(range) + 1;
    if (!is_block_free(rt, next_block_no))
        return ERR_NOT_SUPPORTED;

    // so that block is free, we can use it.
    mark_block_used(rt, next_block_no);
    rt->cache->wipe(rt->cache, next_block_no);
    range->blocks_count++;
    *block_no = next_block_no;
    return OK;
}

// ------------------------------------------------------------------

// given an inode, it resolves the relative block index in the file, into the block_no on the disk
static int find_block_no_from_file_block_index(runtime_data *rt, inode *file_inode, uint32_t block_index_in_file, uint32_t *absolute_block_no) {

    // first, try the inline ranges
    for (int i = 0; i < RANGES_IN_INODE; i++) {
        if (!is_range_used(&file_inode->ranges[i]))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&file_inode->ranges[i], &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if there is no extra ranges block, we cannot find it
    if (file_inode->indirect_ranges_block_no == 0)
        return ERR_OUT_OF_BOUNDS;
    
    int ranges_in_block = rt->superblock->block_size_in_bytes / sizeof(block_range);
    block_range range;
    for (int i = 0; i < ranges_in_block; i++) {
        int err = rt->cache->read(rt->cache, file_inode->indirect_ranges_block_no, sizeof(block_range) * i, &range, sizeof(block_range));
        if (err != OK) return err;

        if (!is_range_used(&range))
            return ERR_OUT_OF_BOUNDS;
        if (check_or_consume_blocks_in_range(&range, &block_index_in_file, absolute_block_no))
            return OK;
    }

    // if here, we have exhausted all ranges, relative_block is outside of them all
    // it means we need more ranges, or two-step ranges for bigger files. but... for another day.
    return ERR_OUT_OF_BOUNDS;
}

static int add_block_to_array_of_ranges(runtime_data *rt, block_range *ranges_array, int ranges_count, int fallback_available, int *use_fallback, uint32_t *new_block_no) {
    int last_used_idx;
    int first_free_idx;
    int err;

    // by default, we won't need to use the fallback.
    *use_fallback = 0;

    find_last_used_and_first_free_range(ranges_array, ranges_count, &last_used_idx, &first_free_idx);
    if (first_free_idx >= 0) {

        // if there is one already use, try to extend it.
        if (last_used_idx >= 0) {
            err = extend_range_by_allocating_block(rt, &ranges_array[last_used_idx], new_block_no);
            // if successful, we are good
            if (err == OK) return OK;
        }

        // there is nothing to extend, or we failed to extend. allocate the free one
        err = initialize_range_by_allocating_block(rt, &ranges_array[first_free_idx], new_block_no);
        if (err != OK) return err;

        return OK;
    }

    // so, no free ranges are available, we may have an indirect block though
    // only if there is one last used, and there is no indirect, there is a reason to try to extend
    if (!fallback_available && last_used_idx >= 0) {
        err = extend_range_by_allocating_block(rt, &ranges_array[last_used_idx], new_block_no);
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

static int add_data_block_to_file(runtime_data *rt, inode *inode, uint32_t *absolute_block_no) {
    int err;
    int use_indirect_block;

    err = add_block_to_array_of_ranges(rt, 
        inode->ranges, RANGES_IN_INODE, 
        inode->indirect_ranges_block_no != 0, &use_indirect_block, 
        absolute_block_no);
    if (err == OK && !use_indirect_block) return OK;
    
    // so either we failed, or we need the indirect block
    if (!use_indirect_block)
        return err;
    
    // load or create the indirect block?
    if (inode->indirect_ranges_block_no == 0) {
        err = find_any_free_block(rt, &inode->indirect_ranges_block_no);
        if (err != OK) return err;
        mark_block_used(rt, inode->indirect_ranges_block_no);
        err = rt->cache->wipe(rt->cache, inode->indirect_ranges_block_no);
        if (err != OK) return err;
    } else {
        err = rt->cache->read(rt->cache, 
            inode->indirect_ranges_block_no, 0,
            rt->scratch_block_buffer,
            rt->superblock->block_size_in_bytes);
        if (err != OK) return err;
    }

    err = add_block_to_array_of_ranges(rt, 
        (block_range *)rt->scratch_block_buffer, 
        rt->superblock->block_size_in_bytes / sizeof(block_range), 
        0, // no fallback exists
        &use_indirect_block, // actualy, we don't care
        absolute_block_no);
    if (err != OK) return err;

    // write back the changes
    err = rt->cache->write(rt->cache, 
        inode->indirect_ranges_block_no, 0,
        rt->scratch_block_buffer,
        rt->superblock->block_size_in_bytes);
    if (err != OK) return err;

    // finally! this was a lot!
    return OK;
}

static int read_inode_from_inodes_database(filesys_data *data, int inode_no, inode_in_mem *target) {
    int err;

    // find the block where this inode is stored in
    uint32_t inodes_per_block = data->runtime->superblock->block_size_in_bytes / sizeof(inode);
    uint32_t block_index_in_file = inode_no / inodes_per_block;
    uint32_t disk_block_no = 0;
    err = find_block_no_from_file_block_index(data->runtime, &data->runtime->superblock->inodes_db_inode, block_index_in_file, &disk_block_no);
    if (err != OK) return err;

    // now we know the absolute block, we can read it
    uint32_t offset_in_block = (inode_no % inodes_per_block) * sizeof(inode);
    err = data->runtime->cache->read(data->runtime->cache, disk_block_no, offset_in_block, &target->inode, sizeof(inode));
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

static int sfs_mkfs(simplest_filesystem *sfs, char *volume_label) {
    // prepare a temp superblock, then save it.
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_fsck(simplest_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_mount(simplest_filesystem *sfs, int readonly) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->runtime != NULL)
        return ERR_NOT_SUPPORTED;

    // first check the first sector to see if we can mount it.
    // hence why the superblock must be at most 512 bytes size.
    uint8_t *temp_sector = data->memory->allocate(data->memory, data->device->get_sector_size(data->device));
    data->device->read_sector(data->device, 0, temp_sector);
    superblock *tmpsb = (superblock *)temp_sector;
    int recognized = memcmp(tmpsb->magic, "SFS1", 4) == 0;
    if (!recognized) {
        data->memory->release(data->memory, temp_sector);
        return ERR_NOT_RECOGNIZED;
    }

    // ok, we are safe to load, prepare memory
    runtime_data *runtime = data->memory->allocate(data->memory, sizeof(runtime_data));
    runtime->readonly = readonly;
    runtime->superblock = data->memory->allocate(data->memory, tmpsb->block_size_in_bytes);
    runtime->used_blocks_bitmap = data->memory->allocate(data->memory, tmpsb->used_blocks_bitmap_blocks_count * tmpsb->block_size_in_bytes);
    runtime->scratch_block_buffer = data->memory->allocate(data->memory, tmpsb->block_size_in_bytes);
    runtime->opened_inodes_in_mem = data->memory->allocate(data->memory, sizeof(inode_in_mem) * MAX_OPEN_FILES);
    runtime->cache = new_cache_layer(data->memory, data->device, tmpsb->block_size_in_bytes, 64);
    data->runtime = runtime;
    
    // read superblock, low level, since we are unmounted
    int err = read_block_range_low_level(
        data->device,
        tmpsb->sector_size,
        tmpsb->sectors_per_block,
        0, // first block
        1, // block count
        data->runtime->superblock
    );

    // make sure to free early, before anything else
    data->memory->release(data->memory, temp_sector);
    if (err != OK) return err;

    // read used blocks bitmap from disk
    err = read_block_range_low_level(
        data->device, 
        data->runtime->superblock->sector_size,
        data->runtime->superblock->sectors_per_block,
        data->runtime->superblock->used_blocks_bitmap_first_block, 
        data->runtime->superblock->used_blocks_bitmap_blocks_count, 
        data->runtime->used_blocks_bitmap
    );
    if (err != OK) return err;

    return OK;
}

static int sfs_sync(simplest_filesystem *sfs) {
    int err;

    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;

    // write superblock
    err = write_block_range(sfs->sfs_data, 0, 1, data->runtime->superblock);
    if (err != OK) return err;

    // write used blocks bitmap to disk
    err = write_block_range(sfs->sfs_data, 
        data->runtime->superblock->used_blocks_bitmap_first_block, 
        data->runtime->superblock->used_blocks_bitmap_blocks_count, 
        data->runtime->used_blocks_bitmap
    );
    if (err != OK) return err;

    // write the inodes_database and root_directory inodes and handles

    // write all the inodes opened in memory

    // all caches flush
    data->runtime->cache->flush(data->runtime->cache);

    return OK;
}

static int sfs_unmount(simplest_filesystem *sfs) {
    filesys_data *data = (filesys_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;

    if (!data->runtime->readonly) {
        int err = sfs_sync(sfs);
        if (err != OK) return err;
    }

    data->runtime->cache->release_memory(data->runtime->cache);

    data->memory->release(data->memory, data->runtime->superblock);
    data->memory->release(data->memory, data->runtime->used_blocks_bitmap);
    data->memory->release(data->memory, data->runtime->scratch_block_buffer);
    data->memory->release(data->memory, data->runtime->opened_inodes_in_mem);
    data->memory->release(data->memory, data->runtime);
    data->runtime = NULL;

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
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->runtime->superblock;

    uint32_t file_block_index = h->file_position / sb->block_size_in_bytes;
    uint32_t offset_in_block  = h->file_position % sb->block_size_in_bytes;
    int bytes_read = 0;

    while (size > 0) {
        if (h->file_position >= h->inode_in_mem->inode.file_size)
            break;
        
        uint32_t block_on_disk = 0;
        int err = find_block_no_from_file_block_index(data->runtime, &h->inode_in_mem->inode, file_block_index, &block_on_disk);
        if (err != OK) return err;

        // read within the file, within the block
        uint32_t chunk_size = size;
        chunk_size = at_most(chunk_size, sb->block_size_in_bytes - offset_in_block);
        chunk_size = at_most(chunk_size, h->inode_in_mem->inode.file_size - h->file_position);

        // read this chunk from this block
        err = data->runtime->cache->read(data->runtime->cache, block_on_disk, offset_in_block, buffer, chunk_size);
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
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;

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
            err = add_data_block_to_file(data->runtime, &h->inode_in_mem->inode, &block_on_disk);
            if (err != OK) return err;
        } else {
            err = find_block_no_from_file_block_index(data->runtime, &h->inode_in_mem->inode, file_block_index, &block_on_disk);
            if (err != OK) return err;
        }

        // write within the block only
        uint32_t chunk_size = at_most(size, sb->block_size_in_bytes - offset_in_block);

        // write this chunk from this block
        err = data->runtime->cache->write(data->runtime->cache, block_on_disk, offset_in_block, buffer, chunk_size);
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
    sfs_data->runtime = NULL; // we are not mounted, for now

    simplest_filesystem *sfs = memory->allocate(memory, sizeof(simplest_filesystem));
    sfs->sfs_data = sfs_data;

    sfs->mkfs = sfs_mkfs;
    sfs->fsck = sfs_fsck;

    sfs->mount = sfs_mount;
    sfs->sync = sfs_sync;
    sfs->unmount = sfs_unmount;

    sfs->sfs_open = sfs_open;
    sfs->sfs_read = sfs_read;
    sfs->sfs_write = sfs_write;
    sfs->sfs_close = sfs_close;

    sfs->sfs_seek = sfs_seek;
    sfs->sfs_tell = sfs_tell;

    return sfs;
}

// ------------------------------------------------------------------

