#pragma once

#include <stdlib.h>
#include "../dependencies/returns.h"
#include "../dependencies/mem_allocator.h"
#include "../dependencies/sector_device.h"

// --- caching layer -------------

// keeps one or more blocks in memory, reads and writes in memory, flushes as needed
typedef struct cache_layer cache_layer;
struct cache_layer {
    int (*read)(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length);
    int (*write)(cache_layer *cl, uint32_t block_no, int offset_in_block, void *buffer, int length);
    int (*wipe)(cache_layer *cl, uint32_t block_no);  // wipes a block with zeros, in prep for writing
    int (*flush)(cache_layer *cl);
    int (*release_memory)(cache_layer *cl);
    void *data;
};

cache_layer *new_cache_layer(mem_allocator *memory, sector_device *device, int block_size_bytes, int blocks_to_cache);

// -------------------------------------------

typedef struct block_bitmap_data block_bitmap_data;
struct block_bitmap_data {
    mem_allocator *memory;
    char *buffer; // multiple of block size.
    uint32_t tracked_blocks_count;
    uint32_t bitmap_size_in_blocks;
    uint32_t buffer_size;
};

typedef struct block_bitmap block_bitmap;
struct block_bitmap {
    int (*is_block_used)(block_bitmap *bb, uint32_t block_no);
    int (*is_block_free)(block_bitmap *bb, uint32_t block_no);
    void (*mark_block_used)(block_bitmap *bb, uint32_t block_no);
    void (*mark_block_free)(block_bitmap *bb, uint32_t block_no);
    int (*find_a_free_block)(block_bitmap *bb, uint32_t *block_no);
    void (*release_memory)(block_bitmap *bb);
    block_bitmap_data *data;
};

block_bitmap *new_bitmap(mem_allocator *memory, uint32_t tracked_blocks_count, uint32_t bitmap_blocks_count, uint32_t block_size);

// -------------------------------------------

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
typedef struct mounted_data mounted_data;
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
    mounted_data *mounted; // if non-null, the filesystem is mounted
};

struct mounted_data {
    int readonly;
    superblock *superblock;
    cache_layer *cache;
    block_bitmap *bitmap;

    uint8_t *used_blocks_bitmap; // bitmap of used block, mirrored in memory
    uint8_t *scratch_block_buffer;
    inode_in_mem *opened_inodes_in_mem; // array of structs in place, of size MAX_OPEN_FILES
};

struct superblock { // must be up to 512 bytes, in order to read from unknown device
    char magic[4];  // e.g. "SFS1" version can be in here
    uint32_t sector_size;          // typically 512 to 4K, device driven
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t block_allocation_bitmap_first_block;  // typically block 1 (0 is superblock)
    uint32_t block_allocation_bitmap_blocks_count;       // typically 1 through 16 blocks

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

