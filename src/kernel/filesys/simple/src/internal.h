#pragma once

#include <stdlib.h>
#include "../dependencies/returns.h"
#include "../dependencies/mem_allocator.h"
#include "../dependencies/sector_device.h"
#include "../dependencies/clock_device.h"

// -------------------------------------------

/*
    TODO:
    - lower the ranges to use 2 byte for range, test failure of expansion.
    - make indirect blocks chain from last entry of each one.
*/

// -------------------------------------------

#define KB   (1024)
#define MB   (1024*KB)
#define GB   (1024*MB)

#define MAX_FILENAME_LENGTH                  59  // affects directory entry size
#define MAX_OPEN_INODES                     128  // later we can do this dynamic
#define MAX_OPEN_HANDLES                    128  // later we can do this dynamic
#define CACHE_SLOTS                         128  // blocks to keep in memory
#define RANGES_IN_INODE                       6  // affects inode size

#define INODE_DB_INODE_ID            0xFFFFFFFe  // masquerades as inode id
#define ROOT_DIR_INODE_ID            0xFFFFFFFf  // masquerades as inode id

#define ceiling_division(x, y)       (((x) + ((y)-1)) / (y))
#define min(a, b)                    ((b) < (a) ? (b) : (a))
#define at_most(a, b)                ((b) < (a) ? (b) : (a))
#define at_least(a, b)               ((b) > (a) ? (b) : (a))
#define in_range(value, low, hi)     ((value) < (low) ? (low) : ((value) > (hi) ? (hi) : (value)))

// -------------------------------------------------------

typedef struct filesys_data filesys_data;
typedef struct mounted_data mounted_data;
typedef struct stored_superblock stored_superblock;
typedef struct block_range block_range;
typedef struct stored_inode stored_inode;
typedef struct stored_dir_entry stored_dir_entry;
typedef struct block_cache block_cache;
typedef struct block_bitmap block_bitmap;
typedef struct cached_inode cached_inode;
typedef struct open_handle open_handle;

/**
 * a pair of starting block and blocks count.
 * used in inodes to define the disk blocks the file occupies
 */
struct __attribute__((packed)) block_range { // target size: 6
    uint32_t first_block_no;
    uint16_t blocks_count;  // 65k blocks, when 512->32mb file, when 4k->256mb file.
};

/**
 * the structure that describes a file. fixed struct size.
 * persisted in the inodes database file
 */
struct stored_inode { // target size: 64
    uint8_t is_used: 1;
    uint8_t is_file: 1;
    uint8_t is_dir:  1;
    uint8_t padding1[3];
    
    uint32_t file_size;     // 32 bits mean max 4GB file size.
    uint32_t allocated_blocks; // 24 bits would be enough, we could use the other 8 for flags
    uint32_t modified_at;  // seconds since epoch, when data was modified.
    // maybe permissions (e.g. xrwxrwxrw), and owner user/group

    uint8_t padding2[4];
    block_range ranges[RANGES_IN_INODE]; // size: 6 * 6 = 36
    uint32_t indirect_ranges_block_no; // this should be aligned to 4 bytes...
    uint32_t double_indirect_block_no; // this should be aligned to 4 bytes...
};

_Static_assert(sizeof(stored_inode) == 64, "stored_inode wrong size");

/**
 * each directory is actually a file of records as the below.
 * stored on disk. fixed size of 64 bytes.
 */
struct __attribute__((packed)) stored_dir_entry {
    char name[MAX_FILENAME_LENGTH + 1];
    uint32_t inode_id;
};

_Static_assert(sizeof(stored_dir_entry) == 64, "stored_dir_entry wrong size");

/**
 * data written in the first sector (512 bytes) and block of the device
 * kept in memory while mounted
 */
struct stored_superblock { // must be up to 512 bytes, in order to read from unknown device
    // offset x000
    char magic[4];                 // e.g. "SFS1" version can be in here
    uint16_t direntry_size;        // currently 64 bytes. to ensure same size when mounting
    uint16_t inode_size;           // currently 64 bytes. to ensure same size when mounting
    uint32_t inodes_db_rec_count;  // how many inodes in inodes_db (includes cleared ones)
    uint32_t dummy1;

    // offset x010
    uint32_t sector_size;          // typically 512 to 4K, device driven
    uint32_t sectors_per_block;    // typically 1-8, for a block size of 512..4kB
    uint32_t block_size_in_bytes;  // typically 512, 1024, 2048 or 4096.
    uint32_t blocks_in_device;     // typically 2k..10M

    // offset 0x020
    uint32_t blocks_bitmap_first_block;   // typically block 1 (0 is superblock)
    uint32_t blocks_bitmap_blocks_count;  // typically 1 through 16 blocks
    uint32_t dummy2;
    uint32_t dummy3;

    // offset 0x030
    stored_inode inodes_db_inode; // file with inodes. inode_no is the record number, zero based.
    // offset 0x070
    stored_inode root_dir_inode;  // file with the entries for root directory. 
    // offset 0x0b0
    char volume_label[32]; 

    // offset 0x0d0
    char dummy4[512
        -4
        -2*sizeof(uint16_t)
        -10*sizeof(uint32_t)
        -2*sizeof(stored_inode)
        -32
    ];
};

_Static_assert(sizeof(stored_superblock) == 512, "stored_superblock wrong size");

/**
 * an in-memory variable for an open inode
 * one per each open unique inode
 */
struct cached_inode {
    uint8_t is_used: 1;      
    uint8_t is_dirty: 1;     // must write this inode to disk

    stored_inode inode;              // buffer to load the inode
    uint32_t inode_id; // which record it is (0xFFFFFFFF is the root dir inode)
    int ref_count;            // how many files have this inode open
    // could contain locking information (e.g. open exclusive)
};

/**
 * an in-memory variable for an open handle. 
 * many processes can open a file, each have their handle, pointing to the same inode
 */
struct open_handle { // the per-process open file handle. two or more can point to same cached_inode
    uint8_t is_used: 1;      
    cached_inode *inode;       
    uint32_t file_position;
    // could contain open information, e.g. append mode
};


struct filesys_data {
    mem_allocator *memory;
    sector_device *device;
    clock_device *clock;
    mounted_data *mounted; // if non-null, the filesystem is mounted
};

/**
 * runtime structure representing all the information of a mounted filesystem.
 */
struct mounted_data {
    int readonly;
    stored_superblock *superblock;
    clock_device *clock;

    block_cache *cache;
    block_bitmap *bitmap;

    cached_inode cached_inodes[MAX_OPEN_INODES];
    cached_inode *cached_inodes_db_inode;
    cached_inode *cached_root_dir_inode;
    int cached_inode_next_eviction; // for round-robin eviction
    
    open_handle open_handles[MAX_OPEN_HANDLES];
    
    uint8_t *generic_block_buffer;
};


// ------------------------------------------------------------------

// block_ops.inc.c
static int read_block(sector_device *dev, uint32_t bytes_per_sector, uint32_t sectors_per_block, uint32_t block_id, void *buffer);
static int write_block(sector_device *dev, uint32_t bytes_per_sector, uint32_t sectors_per_block, uint32_t block_id, void *buffer);

// block_cache.inc.c
static inline block_cache *initialize_block_cache(mem_allocator *mem, sector_device *device, uint32_t block_size);
static inline int bcache_read(block_cache *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int bcache_write(block_cache *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int bcache_wipe(block_cache *data, uint32_t block_no);
static inline int bcache_flush(block_cache *data);
static inline void bcache_release_memory(block_cache *data, mem_allocator *mem);
static void bcache_dump_debug_info(block_cache *data);

// bitmap.inc.c
static block_bitmap *initialize_block_bitmap(mem_allocator *mem, uint32_t storage_first_block, uint32_t bitmap_size_in_blocks, uint32_t blocks_in_device, uint32_t block_size);
static int bitmap_load(block_bitmap *bitmap, block_cache *cache);
static int bitmap_save(block_bitmap *bitmap, block_cache *cache);
static int bitmap_is_block_used(block_bitmap *bitmap, uint32_t block_no);
static int bitmap_is_block_free(block_bitmap *bitmap, uint32_t block_no);
static inline void bitmap_mark_block_used(block_bitmap *bitmap, uint32_t block_no);
static inline void bitmap_mark_block_free(block_bitmap *bitmap, uint32_t block_no);
static int bitmap_find_free_block(block_bitmap *bitmap, uint32_t *block_no);
static void bitmap_release_memory(block_bitmap *bitmap, mem_allocator *mem);
static void bitmap_dump_debug_info(block_bitmap *bitmap);

// ranges.inc.c
static int range_array_resolve_index(block_range *arr, int items, uint32_t *block_index, uint32_t *block_no);
static int range_array_last_block_no(block_range *arr, int items, uint32_t *block_no);
static int range_array_expand(block_range *arr, int items, block_bitmap *bitmap, uint32_t *block_no, int *overflown);
static void range_array_release_blocks(block_range *arr, int items, block_bitmap *bitmap);

// range_blocks.inc.c
static int range_block_get_last_block_no(mounted_data *mt, uint32_t range_block_no, uint32_t *last_block_no);
static int range_block_initialize(mounted_data *mt, uint32_t range_block_no, uint32_t *new_block_no);
static int range_block_expand(mounted_data *mt, uint32_t range_block_no, uint32_t *new_block_no, int *overflown);
static int range_block_release_indirect_block(mounted_data *mt, uint32_t indirect_block_no);
static int range_block_release_dbl_indirect_block(mounted_data *mt, uint32_t dbl_indirect_block_no);
static int range_block_resolve_indirect_index(mounted_data *mt, uint32_t indirect_block_no, uint32_t *block_index, uint32_t *block_no);
static int range_block_resolve_dbl_indirect_index(mounted_data *mt, uint32_t dbl_indirect_block_no, uint32_t *block_index, uint32_t *block_no);

// inode_ops.inc.c - operations an inode can do
static int inode_read_file_bytes(mounted_data *mt, cached_inode *n, uint32_t file_pos, void *data, uint32_t length);
static int inode_write_file_bytes(mounted_data *mt, cached_inode *n, uint32_t file_pos, void *data, uint32_t length);
static int inode_read_file_rec(mounted_data *mt, cached_inode *n, uint32_t rec_size, uint32_t rec_no, void *rec);
static int inode_write_file_rec(mounted_data *mt, cached_inode *n, uint32_t rec_size, uint32_t rec_no, void *rec);
static int inode_truncate_file_bytes(mounted_data *mt, cached_inode *n);
static int inode_resolve_block(mounted_data *mt, cached_inode *node, uint32_t block_index_in_file, uint32_t *absolute_block_no);
static int inode_extend_file_blocks(mounted_data *mt, cached_inode *node, uint32_t *new_block_no);
static void inode_dump_debug_info(mounted_data *mt, const char *title, stored_inode *n);

// inode_db.inc.c
static int inode_db_rec_count(mounted_data *mt);
static stored_inode inode_prepare(clock_device *clock, int for_dir);
static int inode_db_load(mounted_data *mt, uint32_t inode_id, stored_inode *node);
static int inode_db_append(mounted_data *mt, stored_inode *node, uint32_t *inode_id);
static int inode_db_update(mounted_data *mt, uint32_t inode_id, stored_inode *node);
static int inode_db_delete(mounted_data *mt, uint32_t inode_id);

// superblock.inc.c
static int populate_superblock(const char *label, uint32_t sector_size, uint32_t sector_count, uint32_t desired_block_size, stored_superblock *sb);
static void superblock_dump_debug_info(stored_superblock *sb);

// inode_cache.inc.c
static int get_cached_inode(mounted_data *mt, int inode_id, cached_inode **ptr);
static int icache_invalidate_inode(mounted_data *mt, int inode_id); // when inode is deleted
static int icache_flush_all(mounted_data *mt);
static int icache_is_inode_cached(mounted_data *mt, int inode_id); // for debugging purposes
static void icache_dump_debug_info(mounted_data *mt);

// open.inc.c
static int opened_handles_release(mounted_data *mt, open_handle *handle);
static int opened_files_register(mounted_data *mt, cached_inode *node, open_handle **handle_ptr);
static void opened_files_dump_debug_info(mounted_data *mt);

// dirs.inc.c
static int dir_entries_count(cached_inode *dir_inode);
static int dir_load_entry(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no, stored_dir_entry *entry);
static int dir_find_entry_by_name(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t *inode_id, uint32_t *entry_rec_no);
static int dir_ensure_entry_is_missing(mounted_data *mt, cached_inode *dir_inode, const char *name);
static int dir_update_entry(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no, const char *name, uint32_t inode_id);
static int dir_append_entry(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t inode_id);
static int dir_delete_entry(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no);
static void dir_dump_debug_info(mounted_data *mt, cached_inode *dir_inode, int depth);

// path.inc.c
static void path_get_first_part(const char *path, char *filename_buffer, int filename_buffer_size);
static const char *path_get_last_part(const char *path);

// resolution.inc.c
static int resolve_path_to_inode(mounted_data *mt, const char *path, cached_inode **cached_inode_ptr);
static int resolve_path_parent_to_inode(mounted_data *mt, const char *path, cached_inode **cached_inode_ptr);
