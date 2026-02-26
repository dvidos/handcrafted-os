#pragma once
#include "../../../include/ctypes.h"
#include "../../../klib/string.h"
#include "../../../klib/bitmap.h"
#include "../../../klib/backed_cache.h"
#include "../../../devices/block/block_device.h"



typedef uint32_t block_no_t;
typedef uint32_t inode_no_t;

typedef struct stored_superblock stored_superblock;
typedef struct block_range block_range;
typedef struct stored_inode stored_inode;
typedef struct stored_dir_entry stored_dir_entry;
typedef struct sfs_mount_data sfs_mount_data;


#define RANGES_IN_INODE                       4  // affects inode size
#define MAX_FILENAME_LENGTH                  59  // affects directory entry size
#define BLOCKS_CACHE_CAPACITY                64  // 64=256KB, 128=512KB
#define INODES_CACHE_CAPACITY                64
#define MAX_BLOCK_SIZE_BYTES               4096  // this is the biggest we support

#define INVALID_BLOCK_NO             0
#define INVALID_INODE_NO             0xFFFFFFFf

#define INODE_DB_INODE_ID            0xFFFFFFFd  // masquerades as inode id
#define ROOT_DIR_INODE_ID            0xFFFFFFFe  // masquerades as inode id

#define ROUND_UP(number, step)      ((((number) + (step) - 1) / (step)) * (step))
#define MIN(a, b)                   ((a) < (b) ? (a) : (b))


/**
 * a pair of starting block and blocks count.
 * used in inodes to define the disk blocks the file occupies
 */
struct block_range {
    uint32_t first_block_no;
    uint32_t blocks_count;
} __attribute__((packed));




/**
 * Semi-posix compatible, though binary compatibility is not guarranteed
 * Sticky bit is used to denote a used inode rec (although we could derive from type)
 */
#define STORED_INODE_TYPE_FILE       0x8000
#define STORED_INODE_TYPE_DIR        0x4000
#define STORED_INODE_TYPE_CHAR       0x2000
#define STORED_INODE_TYPE_BLOCK      0x6000  // (6=dir+char)
#define STORED_INODE_TYPE_SYM        0xA000  // (a=file+char)
#define STORED_INODE_TYPE_MASK       0xF000  // mask, since some types are combined bits
#define STORED_INODE_PERM_USER_R     0x0100
#define STORED_INODE_PERM_USER_W     0x0080
#define STORED_INODE_PERM_USER_X     0x0040
#define STORED_INODE_PERM_GROUP_R    0x0020
#define STORED_INODE_PERM_GROUP_W    0x0010
#define STORED_INODE_PERM_GROUP_X    0x0008
#define STORED_INODE_PERM_OTHERS_R   0x0004
#define STORED_INODE_PERM_OTHERS_W   0x0002
#define STORED_INODE_PERM_OTHERS_X   0x0001

/**
 * the structure that describes a file. fixed struct size.
 * persisted in the inodes database file
 */
struct stored_inode { // target size: 64
    uint16_t type_perms;       // see STORED_INODE_XXX flags
    uint8_t user_id;
    uint8_t group_id;
    
    uint32_t file_size;        // 32 bits mean max 4GB file size.
    uint32_t allocated_blocks; // 24 bits would be enough, we could use the other 8 for flags
    uint32_t padding;

    uint32_t created_at;   // seconds since epoch, when file was created
    uint32_t modified_at;  // seconds since epoch, when data was modified.

    block_range ranges[RANGES_IN_INODE]; // size: 4 * 8 = 32
    uint32_t indirect_ranges_block_no; // this should be aligned to 4 bytes...
    uint32_t double_indirect_block_no; // this should be aligned to 4 bytes...
} __attribute__((packed));

static inline stored_inode new_stored_inode_file()  { return (stored_inode){ .type_perms = STORED_INODE_TYPE_FILE }; }
static inline stored_inode new_stored_inode_dir()  { return (stored_inode){ .type_perms = STORED_INODE_TYPE_DIR }; }
static inline bool stored_inode_is_dir(stored_inode *n)    { return (n->type_perms & STORED_INODE_TYPE_MASK) == STORED_INODE_TYPE_DIR; }
static inline bool stored_inode_is_file(stored_inode *n)   { return (n->type_perms & STORED_INODE_TYPE_MASK) == STORED_INODE_TYPE_FILE; }
static inline bool stored_inode_is_used(stored_inode *n)   { return (n->type_perms & STORED_INODE_TYPE_MASK) != 0; }
static inline bool stored_inode_is_unused(stored_inode *n) { return (n->type_perms & STORED_INODE_TYPE_MASK) == 0; }

/**
 * each directory is actually a file of records as the below.
 * stored on disk. fixed size of 64 bytes. could be variable...
 */
struct stored_dir_entry {
    char name[MAX_FILENAME_LENGTH + 1];
    uint32_t inode_num;
} __attribute__((packed));

static inline bool stored_dir_entry_is_used(stored_dir_entry *e) { return (e->name[0] != 0); }
static inline bool stored_dir_entry_is_unused(stored_dir_entry *e) { return (e->name[0] == 0); }
static inline void stored_dir_entry_mark_unused(stored_dir_entry *e) { e->name[0] = 0; e->inode_num = 0; }



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
} __attribute__((packed));

_Static_assert(sizeof(stored_superblock) == 512, "stored_superblock wrong size");
_Static_assert(sizeof(stored_inode) == 64, "stored_inode wrong size");
_Static_assert(sizeof(stored_dir_entry) == 64, "stored_dir_entry wrong size");


struct sfs_mount_data {
    block_device_t *dev;

    bool is_readonly;
    stored_superblock *superblock;

    // we need block bitmap and block cache
    bitmap_t *block_bitmap;
    backed_cache_t *block_cache;

    // we need inodes cache for fast resolution
    backed_cache_t *inode_cache;

    // generic block sized buffer for I/O
    uint8_t *generic_block_buffer;

    stored_inode *root_dir;
    stored_inode *inodes_db;
};

static inline bool is_name_reserved(const char *name) { return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0); }


// sfs_block.c
error_t sfs_block_read(sfs_mount_data *fs_data, uint64_t block_no, char *buffer);
error_t sfs_block_write(sfs_mount_data *fs_data, uint64_t block_no, char *buffer);

// sfs_priv_data.c
error_t sfs_create_fs_data(block_device_t *dev, sfs_mount_data **out);
error_t sfs_sync_fs_data(sfs_mount_data *fs_data);
void sfs_destroy_fs_data(sfs_mount_data *fs_data);

// sfs_inode_ops.c
ssize_t sfs_node_read_file_bytes(sfs_mount_data *mt, stored_inode *ind, uint64_t file_pos, void *data, size_t length);
ssize_t sfs_node_write_file_bytes(sfs_mount_data *mt, stored_inode *ind, inode_no_t inode_num, uint64_t file_pos, const void *data, size_t length);
ssize_t sfs_node_read_file_rec(sfs_mount_data *mt, stored_inode *ind, size_t rec_size, uint32_t rec_no, void *rec);
ssize_t sfs_node_write_file_rec(sfs_mount_data *mt, stored_inode *ind, inode_no_t inode_num, size_t rec_size, uint32_t rec_no, const void *rec);
// ---
error_t sfs_node_dir_find_entry(sfs_mount_data *mt, stored_inode *dir_node, const char *name, uint32_t *rec_no, inode_no_t *inode_no);
error_t sfs_node_dir_set_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, uint32_t rec_no, const char *name, inode_no_t inode_no);
error_t sfs_node_dir_add_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, const char *name, inode_no_t inode_no);
error_t sfs_node_dir_is_empty(sfs_mount_data *mt, stored_inode *sin, bool ignore_special_dirs, bool *is_empty);
// ---
error_t sfs_inodes_db_append(sfs_mount_data *mt, stored_inode *sin, inode_no_t *inode_no);


// sfs_data_blocks.c
error_t sfs_node_resolve_data_block(sfs_mount_data *mt, stored_inode *sin, uint32_t block_index, block_no_t *block_no);
error_t sfs_node_expand_data_blocks(sfs_mount_data *md, stored_inode *sin, inode_no_t inode_num);
error_t sfs_node_release_all_data_blocks(sfs_mount_data *md, block_no_t inode_num);




