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
    - tracks content in extents, i.e lists of pairs of block number + blocks count
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

struct simplest_filesystem_data {
    sector_device *device;
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t block_availability_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t block_availability_bitmap_blocks;       // typically 1 through 16 blocks

    // if non-null, the filesystem is mounted
    runtime_data *runtime;
};

struct runtime_data {
    int readonly;
    superblock *superblock;
    uint8_t *block_availability_bitmap;
};

struct superblock { // must be <= 512 bytes
    char magic[4];  // e.g. "SFS1" version can be in here
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
    sfs_data->block_availability_bitmap_blocks = ceiling_division(bitmap_bytes, sfs_data->block_size_in_bytes);
    sfs_data->block_availability_bitmap_first_block = 1;

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

static int read_block(simplest_filesystem_data *sfs, int block_no, void *buffer) {
    if (block_no < 0 || block_no >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    for (int i = 0; i < sfs->sectors_per_block; i++) {
        int sector_no = (block_no * sfs->sectors_per_block) + i;
        int err = sfs->device->read_sector(sfs->device, sector_no, buffer);
        if (err != OK) return err;
    }
    return OK;
}

static int write_block(simplest_filesystem_data *sfs, int block_no, void *buffer) {
    if (block_no < 0 || block_no >= sfs->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    for (int i = 0; i < sfs->sectors_per_block; i++) {
        int sector_no = (block_no * sfs->sectors_per_block) + i;
        int err = sfs->device->write_sector(sfs->device, sector_no, buffer);
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
    return data->runtime->block_availability_bitmap[byte_no] & (0x01 << bit_no);
}

static int mark_block_used(simplest_filesystem_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= data->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->block_availability_bitmap[byte_no] |= (0x01 << bit_no);
    return OK;
}

static int mark_block_free(simplest_filesystem_data *data, uint32_t block_no) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (block_no >= data->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;
    int byte_no = block_no / 8;
    int bit_no = block_no % 8;
    data->runtime->block_availability_bitmap[byte_no] &= ~(0x01 << bit_no);
    return OK;
}

// -------------------------------------------------------------

static int sfs_mkfs(simplest_filesystem *sfs, char *volume_label) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_fsck(simplest_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
}

// -------------------------------------------------------------

static int sfs_mount(simplest_filesystem *sfs, int readonly) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime != NULL)
        return ERR_NOT_SUPPORTED;

    runtime_data *runtime = malloc(sizeof(runtime_data));
    runtime->readonly = readonly;
    runtime->superblock = malloc(data->block_size_in_bytes);
    runtime->block_availability_bitmap = malloc(data->block_availability_bitmap_blocks * data->block_size_in_bytes);
    data->runtime = runtime;
    
    // read superblock
    int err = read_block(sfs, 0, data->runtime->superblock);
    if (err != OK) return err;

    // read availability bitmaps from disk
    for (int block_no = 0; block_no < data->block_availability_bitmap_blocks; block_no++) {
        uint8_t *block_addr = data->runtime->block_availability_bitmap + (data->block_size_in_bytes * block_no);
        err = read_block(sfs, data->block_availability_bitmap_first_block + block_no, block_addr);
        if (err != OK) return err;
    }

    return OK;
}

static int sfs_sync(simplest_filesystem *sfs) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly)
        return ERR_NOT_SUPPORTED;

    // save superblock
    int err = write_block(sfs, 0, data->runtime->superblock);
    if (err != OK) return err;

    // write availability bitmaps to disk
    for (int block_no = 0; block_no < data->block_availability_bitmap_blocks; block_no++) {
        uint8_t *block_addr = data->runtime->block_availability_bitmap + (data->block_size_in_bytes * block_no);
        err = write_block(sfs, data->block_availability_bitmap_first_block + block_no, block_addr);
        if (err != OK) return err;
    }

    return OK;
}

static int sfs_unmount(simplest_filesystem *sfs) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;

    int err = sync(sfs);
    if (err != OK) return err;

    free(data->runtime->superblock);
    free(data->runtime->block_availability_bitmap);
    free(data->runtime);
    data->runtime = NULL;
    return OK;
}

// -------------------------------------------------------------
































