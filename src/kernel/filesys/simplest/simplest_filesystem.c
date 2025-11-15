#include "returns.h"
#include "simplest_filesystem.h"
#include <stdlib.h>
#include <stdint.h>

#define ceiling_division(x, y)    (((x) + ((y)-1)) / (y))

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

*/


#define KB   (1024)
#define MB   (1024*KB)
#define GB   (1024*MB)
#define data(sfs)     ((simplest_filesystem_data *)sfs->sfs_data)

// -------------------------------------------------------

typedef struct simplest_filesystem_data simplest_filesystem_data;
typedef struct runtime_data runtime_data;
typedef struct superblock superblock;
typedef struct block_range block_range;

struct simplest_filesystem_data {
    sector_device *device;
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t used_blocks_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t used_blocks_bitmap_blocks_count;       // typically 1 through 16 blocks

    // if non-null, the filesystem is mounted
    runtime_data *runtime;
};

struct runtime_data {
    int readonly;
    superblock *superblock;
    uint8_t *used_blocks_bitmap;
    uint8_t *scratch_block_buffer;
};

struct superblock { // must be <= 512 bytes
    char magic[4];  // e.g. "SFS1" version can be in here
    // we need inode for the inodes file
};

struct block_range { // size: 8
    uint32_t first_block_no;
    uint32_t blocks_count;
};

struct inode {
    uint32_t flags; // is unused, is file, is dir, etc.
    uint32_t file_size;
    block_range ranges[6]; // size: 48
    // if internal ranges are not enough, this block is full or ranges
    uint32_t indirect_ranges_block_no;
};

simplest_filesystem *new_simplest_filesystem(sector_device *device) {

    // be flexible, pick a reasonable block size, according to capacity & sector size
    uint32_t sectors = device->get_sector_count(device);
    uint32_t sect_size = device->get_sector_size(device);
    uint64_t capacity = (uint64_t)sectors * (uint64_t)sect_size;
    uint32_t blk_size;
    if      (capacity <=  2*MB) blk_size = 512;
    else if (capacity <=  8*MB) blk_size = 1*KB;
    else if (capacity <= 32*MB) blk_size = 2*KB;
    else                   blk_size = 4*KB;
    if (blk_size < sect_size) blk_size = sect_size;
    if (blk_size % sect_size != 0)
        return ERR_NOT_SUPPORTED;
    
    /*
        Disk Capacity                     2MB     8MB     32MB    1GB
        -------------------------------------------------------------
        Block size (bytes)                512      1K      2K      4K
        Total blocks                     4096    8192   16384  262144
        Blocks for used/free bitmaps        1       1       1       8
        Availability bitmap size (bytes)  512      1K      2K     32K
    */

    simplest_filesystem_data *sfs_data = malloc(sizeof(simplest_filesystem_data));

    sfs_data->block_size_in_bytes = blk_size;
    sfs_data->sectors_per_block = blk_size / sect_size;
    sfs_data->blocks_in_device = (uint32_t)(capacity / blk_size);

    // blocks needed for bitmaps to track used/free blocks
    uint32_t bitmap_bytes = ceiling_division(sfs_data->blocks_in_device, 8);
    sfs_data->used_blocks_bitmap_blocks_count = ceiling_division(bitmap_bytes, sfs_data->block_size_in_bytes);
    sfs_data->used_blocks_bitmap_first_block = 1;

    // we are not mounted
    sfs_data->runtime = NULL;

    simplest_filesystem *sfs = malloc(sizeof(simplest_filesystem));
    sfs->mkfs = sfs_mkfs;
    sfs->mount = sfs_mount;
    sfs->sync = sfs_sync;
    sfs->unmount = sfs_unmount;
    sfs->sfs_data = sfs_data;

    return sfs;
}

// -------------------------------------------------------

static int read_block_range(simplest_filesystem_data *sfs, uint32_t first_block, uint32_t block_count, void *buffer) {
    if (sfs->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (first_block >= sfs->blocks_in_device || first_block + block_count - 1 >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int first_sector = first_block * sfs->sectors_per_block;
    int sector_count = block_count * sfs->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = sfs->device->read_sector(sfs->device, first_sector + i, buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int write_block_range(simplest_filesystem_data *sfs, uint32_t first_block, uint32_t block_count, void *buffer) {
    if (sfs->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (sfs->runtime->readonly == NULL)
        return ERR_NOT_PERMITTED;
    if (first_block >= sfs->blocks_in_device || first_block + block_count - 1 >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int first_sector = first_block * sfs->sectors_per_block;
    int sector_count = block_count * sfs->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = sfs->device->write_sector(sfs->device, first_sector + i, buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int copy_block_range(simplest_filesystem_data *sfs, uint32_t source_first_block, uint32_t dest_first_block, uint32_t block_count) {
    if (sfs->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (sfs->runtime->readonly == NULL)
        return ERR_NOT_PERMITTED;
    if (source_first_block >= sfs->blocks_in_device || source_first_block + block_count - 1 >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    if (dest_first_block >= sfs->blocks_in_device || dest_first_block + block_count - 1 >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    if (source_first_block == dest_first_block || block_count == 0)
        return OK;

    int source_first_sector = source_first_block * sfs->sectors_per_block;
    int dest_first_sector = dest_first_block * sfs->sectors_per_block;
    int sector_count = block_count * sfs->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = sfs->device->read_sector(sfs->device, source_first_sector + i, sfs->runtime->scratch_block_buffer);
        if (err != OK) return err;
        int err = sfs->device->write_sector(sfs->device, dest_first_block + i, sfs->runtime->scratch_block_buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int is_block_used(simplest_filesystem_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= data->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    return data->runtime->used_blocks_bitmap[byte_no] & (0x01 << bit_no);
}

static int mark_block_used(simplest_filesystem_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    if (block_no >= data->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->used_blocks_bitmap[byte_no] |= (0x01 << bit_no);
    return OK;
}

static int mark_block_free(simplest_filesystem_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;
    if (block_no >= data->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->used_blocks_bitmap[byte_no] &= ~(0x01 << bit_no);
    return OK;
}

// -------------------------------------------------------------

static int sfs_mkfs(simplest_filesystem *sfs, char *volume_label) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_fsck(simplest_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_mount(simplest_filesystem *sfs, int readonly) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime != NULL)
        return ERR_NOT_SUPPORTED;

    runtime_data *runtime = malloc(sizeof(runtime_data));
    runtime->readonly = readonly;
    runtime->superblock = malloc(data->block_size_in_bytes);
    runtime->used_blocks_bitmap = malloc(data->used_blocks_bitmap_blocks_count * data->block_size_in_bytes);
    runtime->scratch_block_buffer = malloc(data->block_size_in_bytes);
    data->runtime = runtime;
    
    // read superblock
    int err = read_block_range(sfs, 0, 1, data->runtime->superblock);
    if (err != OK) return err;

    // read used blocks bitmap from disk
    err = read_block_range(sfs, data->used_blocks_bitmap_first_block, data->used_blocks_bitmap_blocks_count, data->runtime->used_blocks_bitmap);
    if (err != OK) return err;

    return OK;
}

static int sfs_sync(simplest_filesystem *sfs) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_PERMITTED;

    // write superblock
    int err = write_block_range(sfs, 0, 1, data->runtime->superblock);
    if (err != OK) return err;

    // write used blocks bitmap to disk
    int err = write_block_range(sfs, data->used_blocks_bitmap_first_block, data->used_blocks_bitmap_blocks_count, data->runtime->used_blocks_bitmap);
    if (err != OK) return err;

    return OK;
}

static int sfs_unmount(simplest_filesystem *sfs) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;

    if (!data->runtime->readonly) {
        int err = sync(sfs);
        if (err != OK) return err;
    }

    free(data->runtime->superblock);
    free(data->runtime->used_blocks_bitmap);
    free(data->runtime->scratch_block_buffer);
    free(data->runtime);
    data->runtime = NULL;

    return OK;
}

// -------------------------------------------------------------
































