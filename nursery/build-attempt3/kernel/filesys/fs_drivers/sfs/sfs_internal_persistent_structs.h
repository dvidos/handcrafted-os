#pragma once
/**
 * This header allows the sharing of the definitions of the Simple File System driver
 * with an external command line tool, that can create the image.
 */




 #include "../../../include/uapi/base.h"

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


/**
 * each directory is actually a file of records as the below.
 * stored on disk. fixed size of 64 bytes. could be variable...
 */
struct stored_dir_entry {
    char name[MAX_FILENAME_LENGTH + 1];
    uint32_t inode_num;
} __attribute__((packed));


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



