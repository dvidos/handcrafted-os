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

// -------------------------------------------------------

typedef struct simplest_filesystem_data simplest_filesystem_data;
typedef struct runtime_data runtime_data;
typedef struct superblock superblock;
typedef struct block_range block_range;
typedef struct inode inode;

struct simplest_filesystem_data {
    sector_device *device;
    runtime_data *runtime; // if non-null, the filesystem is mounted
};

struct runtime_data {
    int readonly;
    superblock *superblock;
    uint8_t *used_blocks_bitmap;
    uint8_t *scratch_block_buffer;
};

struct superblock { // must be up to 512 bytes, in order to read from unknown device
    char magic[4];  // e.g. "SFS1" version can be in here
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t used_blocks_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t used_blocks_bitmap_blocks_count;       // typically 1 through 16 blocks

    uint8_t padding[512 - 4 - (sizeof(uint32_t) * 5) - (sizeof(inode) * 2)];

    // these two 2*64=128 bytes, still 1/4 of a block...
    inode inodes_database; // file with inodes. inode_no is the record number, zero based.
    inode root_directory;  // file with the entries for root directory. 
};

struct inode { // target size: 64
    uint8_t flags; // 0=unused, 1=file, 2=dir, etc.
    // maybe permissions (e.g. xrwxrwxrw), and owner user/group
    // maybe creation and/or modification date, uint32, secondsfrom epoch
    uint8_t padding[7];
    uint32_t file_size; // 32 bits mean max 4GB file size.
    block_range ranges[6]; // size: 48
    // if internal ranges are not enough, this block is full or ranges
    uint32_t indirect_ranges_block_no;
};

struct block_range { // target size: 8
    uint32_t first_block_no;
    uint32_t blocks_count;
};

// ------------------------------------------------------------------

simplest_filesystem *new_simplest_filesystem(sector_device *device) {

    simplest_filesystem_data *sfs_data = malloc(sizeof(simplest_filesystem_data));
    sfs_data->device = device;
    sfs_data->runtime = NULL; // we are not mounted, for now

    simplest_filesystem *sfs = malloc(sizeof(simplest_filesystem));
    sfs->mkfs = sfs_mkfs;
    sfs->mount = sfs_mount;
    sfs->sync = sfs_sync;
    sfs->unmount = sfs_unmount;
    sfs->sfs_data = sfs_data;

    return sfs;
}

// ------------------------------------------------------------------

static int initialize_superblock(uint32_t sector_size, uint32_t sector_count, superblock *superblock) {
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
    
    superblock->block_size_in_bytes = blk_size;
    superblock->sectors_per_block = blk_size / sector_size;
    superblock->blocks_in_device = (uint32_t)(capacity / blk_size);

    // blocks needed for bitmaps to track used/free blocks
    uint32_t bitmap_bytes = ceiling_division(superblock->blocks_in_device, 8);
    superblock->used_blocks_bitmap_blocks_count = ceiling_division(bitmap_bytes, superblock->block_size_in_bytes);
    superblock->used_blocks_bitmap_first_block = 1;

    return OK;
}

static int read_block_range(sector_device *dev, superblock *sb, uint32_t first_block, uint32_t block_count, void *buffer) {
    // very simple to allow us loading the superblock on mounting
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (first_block >= sb->blocks_in_device || first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int first_sector = first_block * sb->sectors_per_block;
    int sector_count = block_count * sb->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = dev->read_sector(dev, first_sector + i, buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int write_block_range(simplest_filesystem_data *data, uint32_t first_block, uint32_t block_count, void *buffer) {
    if (data->runtime == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly == NULL)
        return ERR_NOT_PERMITTED;
    superblock *sb = data->runtime->superblock;
    if (sb == NULL)
        return ERR_NOT_SUPPORTED;
    if (first_block >= sb->blocks_in_device || first_block + block_count - 1 >= sb->blocks_in_device)
        return ERR_OUT_OF_BOUNDS;

    int first_sector = first_block * sb->sectors_per_block;
    int sector_count = block_count * sb->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = data->device->write_sector(data->device, first_sector + i, buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int copy_block_range(simplest_filesystem_data *data, uint32_t source_first_block, uint32_t dest_first_block, uint32_t block_count) {
    if (data->runtime == NULL || data->runtime->superblock == NULL)
        return ERR_NOT_SUPPORTED;
    if (data->runtime->readonly == NULL)
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

    int source_first_sector = source_first_block * sb->sectors_per_block;
    int dest_first_sector = dest_first_block * sb->sectors_per_block;
    int sector_count = block_count * sb->sectors_per_block;
    for (int i = 0; i < sector_count; i++) {
        int err = data->device->read_sector(data->device, source_first_sector + i, data->runtime->scratch_block_buffer);
        if (err != OK) return err;
        int err = data->device->write_sector(data->device, dest_first_block + i, data->runtime->scratch_block_buffer);
        if (err != OK) return err;
    }

    return OK;
}

static int is_block_used(simplest_filesystem_data *data, uint32_t block_no) {
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

static int mark_block_used(simplest_filesystem_data *data, uint32_t block_no) {
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

static int mark_block_free(simplest_filesystem_data *data, uint32_t block_no) {
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

static int sfs_mkfs(simplest_filesystem *sfs, char *volume_label) {
    // prepare a temp superblock, then save it.
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_fsck(simplest_filesystem *sfs, int verbose, int repair) {
    return ERR_NOT_IMPLEMENTED;
}

static int sfs_mount(simplest_filesystem *sfs, int readonly) {
    simplest_filesystem_data *data = (simplest_filesystem_data *)sfs->sfs_data;
    if (data->runtime != NULL)
        return ERR_NOT_SUPPORTED;

    // first check the first sector to see if we can mount it.
    // hence why the superblock must be at most 512 bytes size.
    char *temp_sector = malloc(data->device->get_sector_size(data->device));
    data->device->read_sector(data->device, 0, temp_sector);
    superblock *tmpsb = (superblock *)temp_sector;
    int recognized = memcm(tmpsb->magic, "SFS1", 4) == 0;
    if (!recognized) {
        free(temp_sector);
        return ERR_NOT_RECOGNIZED;
    }

    // ok, we are safe to load, prepare memory
    runtime_data *runtime = malloc(sizeof(runtime_data));
    runtime->readonly = readonly;
    runtime->superblock = malloc(tmpsb->block_size_in_bytes);
    runtime->used_blocks_bitmap = malloc(tmpsb->used_blocks_bitmap_blocks_count * tmpsb->block_size_in_bytes);
    runtime->scratch_block_buffer = malloc(tmpsb->block_size_in_bytes);
    data->runtime = runtime;
    
    // read superblock
    int err = read_block_range(data->device, tmpsb, 0, 1, data->runtime->superblock);

    // make sure to free, now that we're done
    free(temp_sector); 
    if (err != OK) return err;

    // read used blocks bitmap from disk
    err = read_block_range(
        data->device, 
        data->runtime->superblock, 
        data->runtime->superblock->used_blocks_bitmap_first_block, 
        data->runtime->superblock->used_blocks_bitmap_blocks_count, 
        data->runtime->used_blocks_bitmap
    );
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
    int err = write_block_range(sfs, 
        data->runtime->superblock->used_blocks_bitmap_first_block, 
        data->runtime->superblock->used_blocks_bitmap_blocks_count, 
        data->runtime->used_blocks_bitmap
    );
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

// ------------------------------------------------------------------
































