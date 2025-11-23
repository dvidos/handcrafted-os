#pragma once

#include <stdlib.h>
#include "../dependencies/returns.h"
#include "../dependencies/mem_allocator.h"
#include "../dependencies/sector_device.h"

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
typedef struct cache_data cache_data;



struct block_range { // target size: 8
    uint32_t first_block_no;
    uint32_t blocks_count;
};

struct inode { // target size: 64
    uint8_t flags; // 0=unused, 1=file, 2=dir, etc.
    // maybe permissions (e.g. xrwxrwxrw), and owner user/group
    // maybe creation and/or modification date, uint32, secondsfrom epoch
    uint8_t padding[3];
    
    uint32_t file_size; // 32 bits mean max 4GB file size.
    uint32_t allocated_blocks; // 24 bits would be enough, we could use the other 8 for flags

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

    uint8_t *used_blocks_bitmap; // bitmap of used block, mirrored in memory
    cache_data *cache;

    uint8_t *scratch_block_buffer;
    inode_in_mem *opened_inodes_in_mem; // array of structs in place, of size MAX_OPEN_FILES
};

struct superblock { // must be up to 512 bytes, in order to read from unknown device
    char magic[4];  // e.g. "SFS1" version can be in here
    uint32_t sector_size;          // typically 512 to 4K, device driven
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M
    uint32_t blocks_bitmap_first_block;   // typically block 1 (0 is superblock)
    uint32_t blocks_bitmap_blocks_count;  // typically 1 through 16 blocks
    uint32_t max_inode_rec_no_in_db; // how many inodes in inodes_db (includes cleared ones)
    char padding1[256 - (7 * sizeof(uint32_t))];

    // these two 2*64=128 bytes, still 1/4 of a block...
    inode inodes_db_inode; // file with inodes. inode_no is the record number, zero based.
    inode root_dir_inode;  // file with the entries for root directory. 
    char padding2[256 - (2*sizeof(inode)) - (1*sizeof(uint32_t))];
};

// ------------------------------------------------------------------

struct inode_in_mem {
    // this is needed to avoid inode operations on disk directly.
    uint32_t flags; // is used, is dirty etc
    uint32_t inode_db_rec_no;
    inode inode; // mirroring what is (or must be) on disk
};

// -------------------------------------------------------------------

static int read_through_cache(mounted_data *mt, int block_no, int block_offset, int length, void *buffer);
static int write_through_cache(mounted_data *mt, int block_no, int block_offset, int length, void *buffer);
static int flush_cache(mounted_data *mt);


// -------------------------------------------------------------------

// superblock
static int populate_superblock(uint32_t sector_size, uint32_t sector_count, uint32_t desired_block_size, superblock *sb);

// ranges.inc.c
static inline int is_range_used(const block_range *range);
static inline int is_range_empty(const block_range *range);
static inline uint32_t range_last_block_no(const block_range *range);
static inline int check_or_consume_blocks_in_range(const block_range *range, uint32_t *relative_block_no, uint32_t *absolute_block_no);
static inline void find_last_used_and_first_free_range(const block_range *ranges_array, int ranges_count, int *last_used_idx, int *first_free_idx);
static inline int initialize_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no);
static inline int extend_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no);

// blocks.inc.c
static int read_block_range_low_level(sector_device *dev, uint32_t sector_size, uint32_t sectors_per_block, uint32_t first_block, uint32_t block_count, void *buffer);
static int read_block(filesys_data *data, uint32_t block_no, void *buffer);
static int write_block(filesys_data *data, uint32_t block_no, void *buffer);
static int find_block_no_from_file_block_index(mounted_data *mt, const inode *inode, uint32_t block_index_in_file, uint32_t *absolute_block_no);
static int add_block_to_array_of_ranges(mounted_data *mt, block_range *ranges_array, int ranges_count, int fallback_available, int *use_fallback, uint32_t *new_block_no);
static int add_data_block_to_file(mounted_data *mt, inode *inode, uint32_t *absolute_block_no);

// inodes.inc.c
static int read_inode_from_inodes_database(mounted_data *mt, int rec_no, inode_in_mem *inmem);
static int write_inode_to_inodes_database(mounted_data *mt, int rec_no, inode_in_mem *inmem);
static int append_inode_to_inodes_database(mounted_data *mt, inode_in_mem *inmem, int *rec_no);
static int delete_inode_from_inodes_database(mounted_data *mt, int rec_no);

// cache.inc.c
static inline cache_data *initialize_cache(mem_allocator *memory, sector_device *device, uint32_t block_size);
static inline int cached_read(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int cached_write(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int cached_wipe(cache_data *data, uint32_t block_no);
static inline int cache_flush(cache_data *data);
static inline void cache_release_memory(cache_data *data);

// if we converted bitmap to methods
static inline int is_block_used(mounted_data *mt, uint32_t block_no);
static inline int is_block_free(mounted_data *mt, uint32_t block_no);
static inline void mark_block_used(mounted_data *mt, uint32_t block_no);
static inline void mark_block_free(mounted_data *mt, uint32_t block_no);
static inline int find_a_free_block(mounted_data *mt, uint32_t *block_no);

// high-level cached file operations, shared for dbs, directories, real files, extend files as needed.
int file_read_data(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length);
int file_write_data(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length);
