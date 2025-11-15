#include "returns.h"
#include "simplest_filesystem.h"
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

// -------------------------------------------------------

typedef struct filesys_data filesys_data;
typedef struct runtime_data runtime_data;
typedef struct superblock superblock;
typedef struct block_range block_range;
typedef struct inode_on_disk inode_on_disk;
typedef struct inode_in_mem inode_in_mem;

struct block_range { // target size: 8
    uint32_t first_block_no;
    uint32_t blocks_count;
};

struct inode_on_disk { // target size: 64
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

    inode_on_disk inodes_db_inode;
    inode_on_disk root_dir_inode;
};

struct superblock { // must be up to 512 bytes, in order to read from unknown device
    char magic[4];  // e.g. "SFS1" version can be in here
    uint32_t sector_size;          // typically 512 to 4K, device driven
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t used_blocks_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t used_blocks_bitmap_blocks_count;       // typically 1 through 16 blocks

    uint8_t padding[512 - 4 - (sizeof(uint32_t) * RANGES_IN_INODE) - (sizeof(inode_on_disk) * 2)];

    // these two 2*64=128 bytes, still 1/4 of a block...
    inode_on_disk inodes_database; // file with inodes. inode_no is the record number, zero based.
    inode_on_disk root_directory;  // file with the entries for root directory. 
};

// ------------------------------------------------------------------

struct inode_in_mem {
    // this is needed to avoid inode operations on disk directly.
    uint32_t flags; // is used, is dirty etc
    uint32_t inode_no;
    inode_on_disk inode; // mirroring what is (or must be) on disk
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

static int is_block_used(filesys_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    superblock *sb = data->runtime->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    return data->runtime->used_blocks_bitmap[byte_no] & (0x01 << bit_no);
}

static int mark_block_used(filesys_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->used_blocks_bitmap[byte_no] |= (0x01 << bit_no);
    return OK;
}

static int mark_block_free(filesys_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->used_blocks_bitmap[byte_no] &= ~(0x01 << bit_no);
    return OK;
}

// ------------------------------------------------------------------

static int find_absolute_block_no(filesys_data *data, inode_on_disk *inode, uint32_t relative_block_no, uint32_t *absolute_block_no) {
    // relative_block_no is zero based here

    for (int i = 0; i < RANGES_IN_INODE; i++) {
        block_range *range = &inode->ranges[i];

        if (range->first_block_no == 0) {
            // we reached an empty range, therefore relative_block_no is out of bounds
            return ERR_OUT_OF_BOUNDS;
        }
        if (relative_block_no < range->blocks_count) {
            // relative block is within this range
            *absolute_block_no = range->first_block_no + relative_block_no;
            return OK;
        }

        // this could not go negative, because we just tested for less.
        relative_block_no -= range->blocks_count;
    }

    // we did not find it within the immediate inode ranges, go to the secondary one
    int err = read_block_range(data, inode->indirect_ranges_block_no, 1, data->runtime->scratch_block_buffer);
    if (err != OK) return err;

    // now do the same
    int ranges_in_block = data->runtime->superblock->block_size_in_bytes / sizeof(block_range);
    for (int i = 0; i < ranges_in_block; i++) {
        block_range *range = (block_range *)(data->runtime->scratch_block_buffer + i * sizeof(block_range));

        if (range->first_block_no == 0) {
            // we reached an empty range, therefore relative_block_no is out of bounds
            return ERR_OUT_OF_BOUNDS;
        }
        if (relative_block_no < range->blocks_count) {
            // relative block is within this range
            *absolute_block_no = range->first_block_no + relative_block_no;
            return OK;
        }

        // this could not go negative, because we just tested for less.
        relative_block_no -= range->blocks_count;
    }

    // if here, we also exhausted all ranges, relative_block is outside of them all
    // it means we need more ranges, or two-step ranges for bigger files.
    return ERR_OUT_OF_BOUNDS;
}

static int read_inode_from_inodes_database(filesys_data *data, int inode_no, inode_in_mem *target) {
    int err;

    // find the block where this inode is stored in
    uint32_t inodes_per_block = data->runtime->superblock->block_size_in_bytes / sizeof(inode_on_disk);
    uint32_t relative_block_no = inode_no / inodes_per_block;
    uint32_t absolute_block_no = 0;
    err = find_absolute_block_no(data, &data->runtime->inodes_db_inode, relative_block_no, &absolute_block_no);
    if (err != OK) return err;

    // now we know the absolute block, we can read it
    err = read_block_range(data, absolute_block_no, 1, data->runtime->scratch_block_buffer);
    if (err != OK) return err;
    uint32_t inode_relative_no = inode_no % inodes_per_block;

    target->flags = 0; // not dirty
    target->inode_no = inode_no;
    memcpy(&target->inode, data->runtime->scratch_block_buffer + (inode_relative_no * sizeof(inode_on_disk)), sizeof(inode_on_disk));
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
        free(temp_sector);
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
    free(temp_sector);
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

    data->runtime->cache->free_memory(data->runtime->cache);

    free(data->runtime->superblock);
    free(data->runtime->used_blocks_bitmap);
    free(data->runtime->scratch_block_buffer);
    free(data->runtime->opened_inodes_in_mem);
    free(data->runtime);
    data->runtime = NULL;

    return OK;
}

// ------------------------------------------------------------------

static sfs_handle *sfs_open(char *filename, int options) {
    return NULL;
}
static int sfs_read(sfs_handle *h, uint32_t size, void *buffer) {
    return ERR_NOT_IMPLEMENTED;
}
static int sfs_write(sfs_handle *h, uint32_t size, void *buffer) {
    return ERR_NOT_IMPLEMENTED;
}
static int sfs_close(sfs_handle *h) {
    return ERR_NOT_IMPLEMENTED;
}

// ------------------------------------------------------------------

simplest_filesystem *new_simplest_filesystem(mem_allocator *memory, sector_device *device) {

    if (sizeof(inode_on_disk) != 64) // written on disk, must be same size
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

    return sfs;
}

// ------------------------------------------------------------------

