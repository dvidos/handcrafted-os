#pragma once

#include <stdlib.h>
#include "../dependencies/returns.h"
#include "../dependencies/mem_allocator.h"
#include "../dependencies/sector_device.h"
#include "../dependencies/clock_device.h"

// -------------------------------------------

/*
    TODO:

    - maintain open inodes and handles (e.g. file handles in mounted_data)
      for the two in-built files: inodes db and root dir.
    - lower the ranges to use 2 byte for range, test failure of expansion.
    - make indirect blocks chain from last entry of each one.
    - make the wash test system. we need it.
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
typedef struct superblock superblock;
typedef struct block_range block_range;
typedef struct inode inode;
typedef struct direntry direntry;
typedef struct cache_data cache_data;
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
struct inode { // target size: 64
    uint8_t is_used: 1;
    uint8_t is_file: 1;
    uint8_t is_dir:  1;
    uint8_t padding1[3];
    
    uint32_t file_size;     // 32 bits mean max 4GB file size.
    uint32_t allocated_blocks; // 24 bits would be enough, we could use the other 8 for flags
    uint32_t modified_at;  // seconds since epoch, when data was modified.
    // maybe permissions (e.g. xrwxrwxrw), and owner user/group

    uint8_t padding2[8];
    block_range ranges[RANGES_IN_INODE]; // size: 6 * 6 = 36
    uint32_t indirect_ranges_block_no; // this should be aligned to 4 bytes...
};

/**
 * each directory is actually a file of records as the below.
 * stored on disk. fixed size of 64 bytes.
 */
struct __attribute__((packed)) direntry {
    char name[MAX_FILENAME_LENGTH + 1];
    uint32_t inode_id;
};

/**
 * data written in the first sector (512 bytes) and block of the device
 * kept in memory while mounted
 */
struct superblock { // must be up to 512 bytes, in order to read from unknown device
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
    inode inodes_db_inode; // file with inodes. inode_no is the record number, zero based.
    // offset 0x070
    inode root_dir_inode;  // file with the entries for root directory. 
    // offset 0x0b0
    char volume_label[32]; 

    // offset 0x0d0
    char dummy4[512
        -4
        -2*sizeof(uint16_t)
        -10*sizeof(uint32_t)
        -2*sizeof(inode)
        -32
    ];
};

/**
 * an in-memory variable for an open inode
 * one per each open unique inode
 */
struct cached_inode {
    uint8_t is_used: 1;      
    uint8_t is_dirty: 1;     // must write this inode to disk

    inode inode_in_mem;              // buffer to load the inode
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
    superblock *superblock;
    clock_device *clock;

    uint8_t *used_blocks_bitmap; // bitmap of used block, mirrored in memory
    uint32_t next_free_block_check; // for round-robin discovery
    cache_data *cache;

    cached_inode cached_inodes[MAX_OPEN_INODES];
    int cached_inode_next_eviction; // for round-robin eviction
    
    open_handle open_handles[MAX_OPEN_HANDLES];
    
    uint8_t *generic_block_buffer;
};


// ------------------------------------------------------------------

// path.inc.c
static void path_get_first_part(const char *path, char *filename_buffer, int filename_buffer_size);
static const char *path_get_last_part(const char *path);

// superblock.inc.c
static int populate_superblock(const char *label, uint32_t sector_size, uint32_t sector_count, uint32_t desired_block_size, superblock *sb);
static void superblock_dump_debug_info(superblock *sb);

// bitmap.inc.c
static int used_blocks_bitmap_load(mounted_data *mt);
static int used_blocks_bitmap_save(mounted_data *mt);
static inline int is_block_used(mounted_data *mt, uint32_t block_no);
static inline int is_block_free(mounted_data *mt, uint32_t block_no);
static inline void mark_block_used(mounted_data *mt, uint32_t block_no);
static inline void mark_block_free(mounted_data *mt, uint32_t block_no);
static inline int find_next_free_block(mounted_data *mt, uint32_t *block_no);
static void bitmap_dump_debug_info(mounted_data *mt);

// ranges.inc.c
static inline int is_range_used(const block_range *range);
static inline int is_range_empty(const block_range *range);
static inline uint32_t range_last_block_no(const block_range *range);
static inline int check_or_consume_blocks_in_range(const block_range *range, uint32_t *relative_block_no, uint32_t *absolute_block_no);
static inline void find_last_used_and_first_free_range(const block_range *ranges_array, int ranges_count, int *last_used_idx, int *first_free_idx);
static inline int initialize_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no);
static inline int extend_range_by_allocating_block(mounted_data *mt, block_range *range, uint32_t *block_no);
static void bitmap_dump_debug_info(mounted_data *mt);
static inline void range_array_release_blocks(mounted_data *mt, block_range *ranges_array, int ranges_count);

// cache.inc.c
static inline cache_data *initialize_cache(mem_allocator *memory, sector_device *device, uint32_t block_size);
static inline int cached_read(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int cached_write(cache_data *data, uint32_t block_no, uint32_t block_offset, void *buffer, int length);
static inline int cached_wipe(cache_data *data, uint32_t block_no);
static inline int cache_flush(cache_data *data);
static inline void cache_release_memory(cache_data *data);
static void cache_dump_debug_info(cache_data *data);

// block_ops.inc.c
static int read_block_range_low_level(sector_device *dev, uint32_t sector_size, uint32_t sectors_per_block, uint32_t first_block, uint32_t block_count, void *buffer);
static int read_block(filesys_data *data, uint32_t block_no, void *buffer);
static int write_block(filesys_data *data, uint32_t block_no, void *buffer);
static int find_block_no_from_file_block_index(mounted_data *mt, const inode *inode, uint32_t block_index_in_file, uint32_t *absolute_block_no);
static int add_block_to_array_of_ranges(mounted_data *mt, block_range *ranges_array, int ranges_count, int fallback_available, int *use_fallback, uint32_t *new_block_no);
static int add_data_block_to_file(mounted_data *mt, inode *inode, uint32_t *absolute_block_no);

// inode_ops.inc.c - high-level cached file operations, shared for dbs, directories, real files, extend files as needed.
static int inode_read_file_bytes(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length);
static int inode_write_file_bytes(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length);
static int inode_truncate_file_bytes(mounted_data *mt, inode *n);
static int inode_load(mounted_data *mt, uint32_t inode_id, inode *node);
static int inode_allocate(mounted_data *mt, int is_file, inode *node, uint32_t *inode_id);
static int inode_persist(mounted_data *mt, uint32_t inode_id, inode *node);
static int inode_delete(mounted_data *mt, uint32_t inode_id);
static void inode_dump_debug_info(const char *title, inode *n);

// inode_cache.inc.c
static int get_cached_inode(mounted_data *mt, int inode_id, cached_inode **ptr);
static int inode_cache_invalidate_inode(mounted_data *mt, int inode_id);
static int inode_cache_flush_all(mounted_data *mt);
static void inode_cache_dump_debug_info(mounted_data *mt);

// open.inc.c
static int opened_handles_release(mounted_data *mt, open_handle *handle);
static int opened_files_register(mounted_data *mt, inode *node, uint32_t inode_id, open_handle **handle_ptr);
static void opened_files_dump_debug_info(mounted_data *mt);


// dirs.inc.c
static int dir_entry_find(mounted_data *mt, inode *dir_inode, const char *name, uint32_t *inode_id, uint32_t *entry_rec_no);
static int dir_entry_update(mounted_data *mt, inode *dir_inode, int entry_rec_no, const char *name, uint32_t inode_id);
static int dir_entry_append(mounted_data *mt, inode *dir_inode, const char *name, uint32_t inode_id);
static void dir_dump_debug_info(mounted_data *mt, inode *dir_inode, int depth);
static int dir_entry_ensure_missing(mounted_data *mt, inode *dir_inode, const char *name);
static int dir_entry_delete(mounted_data *mt, inode *dir_inode, int entry_rec_no);
static void dir_dump_debug_info(mounted_data *mt, inode *dir_inode, int depth);

// resolution.inc.c
static int resolve_path_to_inode(mounted_data *mt, const char *path, inode *target, uint32_t *inode_id);
static int resolve_path_parent_to_inode(mounted_data *mt, const char *path, inode *target, uint32_t *inode_id);
