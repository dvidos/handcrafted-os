#pragma once
#include "../../kernel/filesys/fs_drivers/sfs/sfs_internal_persistent_structs.h"


typedef struct context {
    FILE *img;
    size_t img_size;
    size_t fs_offset;
    stored_superblock superblock;
} context;

#define SECTOR_SIZE   512
#define BLOCK_SIZE   1024   // can be 512, 1K, 2K, 4K...

static inline size_t bits_to_bytes(size_t bits) { return (bits + 7) / 8; }
static inline size_t bytes_to_bits(size_t bytes) { return bytes * 8; }
static inline size_t bytes_to_sectors(size_t bytes) { return (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE; }
static inline size_t sectors_to_bytes(size_t sectors) { return sectors * SECTOR_SIZE; }
static inline size_t bytes_to_blocks(size_t bytes) { return (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE; }
static inline size_t blocks_to_bytes(size_t blocks) { return blocks * BLOCK_SIZE; }
static inline size_t sectors_to_blocks(size_t sectors) { return sectors / (BLOCK_SIZE / SECTOR_SIZE); }
static inline size_t blocks_to_sectors(size_t blocks) { return blocks * (BLOCK_SIZE / SECTOR_SIZE); }
static inline size_t bits_to_blocks(size_t bits) { return bytes_to_blocks(bits_to_bytes(bits)); }


void fatal(const char *fmt, ...);
long parse_size(const char *size_str);

void create_host_file(const char *file_name, size_t img_size);
FILE *open_host_file(const char *file_name);

void write_bootable_mbr(FILE *f);
void write_partition_entry(FILE *f, uint32_t start_lba, uint32_t sectors);
void read_partition_entry(FILE *img, uint32_t *start_lba, uint32_t *sectors);
void read_img_sector(FILE *img, uint32_t sector_no, void *buffer);
void write_img_sector(FILE *img, uint32_t sector_no, void *buffer);

size_t count_entries_in_host_dir(const char *host_dir_path);
size_t get_host_file_size(const char *host_file_path);

void read_fs_block(context *ctx, uint32_t block_no, void *buffer);
void write_fs_block(context *ctx, uint32_t block_no, void *buffer);
void write_fs_block_part(context *ctx, uint32_t block_no, uint32_t offset_in_block, void *buffer, size_t size);

void block_bitmap_create(uint32_t blocks_to_track);
uint32_t block_bitmap_allocate(int count);
void block_bitmap_mark_used(uint32_t block_no);
uint8_t *block_bitmap_get_bytes_ptr(int bitmap_block_no); // for loading and saving

void import_host_file_into_img_sector(FILE *img, size_t sector_no, size_t sector_count, const char *host_file_path);
void persist_dir_entry(context *ctx, stored_inode *parent_dir, size_t rec_no, stored_dir_entry *entry);
void append_entry_to_directory(context *ctx, stored_inode *parent_dir, inode_no_t parent_dir_no, const char *entry_name, inode_no_t entry_no);
void persist_inode(context *ctx, inode_no_t inode_no, stored_inode *inode);
