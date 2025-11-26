#include "internal.h"


static int auto_determine_block_size(uint32_t sector_size, uint64_t capacity) {
    /*
        Disk Capacity (up to)          0..2MB   2..8MB    8..32MB   23MB..1GB
        ---------------------------------------------------------------------
        Block size (bytes)                512       1K         2K          4K
        Total blocks                     4096     8192      16384      262144
        Blocks for used/free bitmaps        1        1          1           8
        Availability bitmap size (bytes)  512       1K         2K         32K
    */

    int block_size = 0;

    if (capacity <= 2 * MB)
        block_size = 512;
    else if (capacity <= 8 * MB)
        block_size = 1024;
    else if (capacity <= 32 * MB)
        block_size = 2048;
    else
        block_size = 4096;
    
    if (block_size % sector_size != 0)
        return block_size = (block_size / sector_size) * sector_size;
    if (block_size < sector_size)
        block_size = sector_size;

    return block_size;
}

static int populate_superblock(uint32_t sector_size, uint32_t sector_count, uint32_t desired_block_size, superblock *sb) {
    memset(sb, 0, sizeof(superblock));

    sb->magic[0] = 'S';
    sb->magic[1] = 'F';
    sb->magic[2] = 'S';
    sb->magic[3] = '1';

    uint32_t blk_size;
    uint64_t capacity = (uint64_t)sector_size * (uint64_t)sector_count;

    if (desired_block_size > 0) {
        if (desired_block_size < sector_size 
            || desired_block_size >= 4096 
            || desired_block_size % sector_size != 0)
            return ERR_NOT_SUPPORTED;
        blk_size = desired_block_size;
    } else {
        blk_size = auto_determine_block_size(sector_size, capacity);
    }

    // basic metrics
    sb->sector_size = sector_size;
    sb->block_size_in_bytes = blk_size;
    sb->sectors_per_block = blk_size / sector_size;
    sb->blocks_in_device = (uint32_t)(capacity / blk_size);

    // some blocks are needed for bitmaps to track used/free blocks
    uint32_t bitmap_bytes = ceiling_division(sb->blocks_in_device, 8);
    sb->blocks_bitmap_blocks_count = ceiling_division(bitmap_bytes, sb->block_size_in_bytes);
    sb->blocks_bitmap_first_block = 1;

    // explicitly, though uselessly
    memset(&sb->inodes_db_inode, 0, sizeof(inode));
    sb->max_inode_rec_no_in_db = 0;
    memset(&sb->root_dir_inode, 0, sizeof(inode));
    sb->root_dir_inode.is_dir = 1;

    return OK;
}

