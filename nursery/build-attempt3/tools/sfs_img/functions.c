#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include "functions.h"




void fatal(const char *fmt, ...) {
    char buffer[256];

    va_list v;
    va_start(v, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, v);
    va_end(v);

    fprintf(stderr, "%s\n", buffer);
    exit(1);
}

long parse_size(const char *size_str) {
    if (!size_str) return 0;
    long size = atol(size_str);
    char multiplier = tolower(size_str[strlen(size_str) - 1]);
    switch (multiplier) {
        case 'k': size *= 1024; break;
        case 'm': size *= 1024 * 1024; break;
        case 'g': size *= 1024 * 1024 * 1024; break;
    }
    return size;
}

void create_host_file(const char *filen_name, size_t img_size) {
    FILE *f = fopen(filen_name, "w+");
    if (f == NULL) fatal("Failed creating img file '%s'", filen_name);

    int block_size = 64 * 1024;
    char *block = malloc(block_size);
    memset(block, 0, block_size);

    size_t remaining = img_size;
    while (remaining > 0) {
        size_t bytes = remaining < block_size ? remaining : block_size;
        fwrite(block, 1, bytes, f);
        remaining -= bytes;
    }

    free(block);
}

FILE *open_host_file(const char *file_name) {
    FILE *f = fopen(file_name, "r+");
    if (f == NULL) fatal("Failed opening img file '%s'", file_name);

    return f;
}

void write_bootable_mbr(FILE *f) {
    fseek(f, 510, SEEK_SET);
    uint16_t sig = 0xAA55;
    fwrite(&sig, 1, 2, f);
}

void write_partition_entry(FILE *f, uint32_t start_lba, uint32_t sectors) {
    uint8_t entry[16] = {0};
    entry[0] = 0x80;          // bootable (0x00 = not bootable)
    entry[4] = 0x7F;          // partition type (example: Linux)
    *(uint32_t*)&entry[8]  = start_lba;
    *(uint32_t*)&entry[12] = sectors;
    fseek(f, 0x1BE, SEEK_SET);
    fwrite(entry, 1, 16, f);
}

void read_partition_entry(FILE *img, uint32_t *start_lba, uint32_t *sectors) {
    uint8_t entry[16];
    fseek(img, 0x1BE, SEEK_SET);
    if (fread(entry, 1, 16, img) != 16) fatal("Failed reading partition entry");
    *start_lba = *(uint32_t*)&entry[8];
    *sectors   = *(uint32_t*)&entry[12];
}

size_t count_entries_in_host_dir(const char *host_dir_path) {
    // count entries (. and .. too) in order to allocate blocks
    DIR *dir = opendir(host_dir_path);
    if (dir == NULL) fatal("error opening dir '%s'", host_dir_path);

    struct dirent *e;
    size_t count = 0;
    while ((e = readdir(dir)) != NULL)
        count++;
    closedir(dir);
    return count;
}

size_t get_host_file_size(const char *host_file_path) {
    FILE *f = fopen(host_file_path, "r");
    if (f == NULL) fatal("Error opening host file '%s'", host_file_path);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);
    return (size_t)file_size;
}

void read_img_sector(FILE *img, uint32_t sector_no, void *buffer) {
    long offset = sector_no * SECTOR_SIZE;
    fseek(img, offset, SEEK_SET);
    size_t read = fread(buffer, 1, SECTOR_SIZE, img);
    if (read != SECTOR_SIZE) fatal("Failed reading sector %lu from image", sector_no);
}

void write_img_sector(FILE *img, uint32_t sector_no, void *buffer) {
    long offset = sector_no * SECTOR_SIZE;
    fseek(img, offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, SECTOR_SIZE, img);
    if (written != SECTOR_SIZE) fatal("Failed writing sector %lu to image", sector_no);
}

void read_fs_block(context *ctx, uint32_t block_no, void *buffer) {
    long offset = ctx->fs_offset + block_no * BLOCK_SIZE;
    fseek(ctx->img, offset, SEEK_SET);
    size_t read = fread(buffer, 1, SECTOR_SIZE, ctx->img);
    if (read != SECTOR_SIZE) fatal("Failed reading FS block %lu from image", block_no);
}

void write_fs_block(context *ctx, uint32_t block_no, void *buffer) {
    long offset = ctx->fs_offset + block_no * BLOCK_SIZE;
    fseek(ctx->img, offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, SECTOR_SIZE, ctx->img);
    if (written != SECTOR_SIZE) fatal("Failed writing FS block %lu to image", block_no);
}

void write_fs_block_part(context *ctx, uint32_t block_no, uint32_t offset_in_block, void *buffer, size_t size) {
    long offset = ctx->fs_offset + block_no * BLOCK_SIZE + offset_in_block;
    fseek(ctx->img, offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, size, ctx->img);
    if (written != size) fatal("Failed writing %d bytes in FS block %lu to image", size, block_no);
}

// ---------------------------------------------

static uint8_t *block_bitmap_buffer;
static uint32_t block_bitmap_blocks_to_track;
static uint32_t block_bitmap_next_free_block;

void block_bitmap_create(uint32_t blocks_to_track) {
    uint32_t bytes_needed = bits_to_bytes(blocks_to_track);
    uint32_t blocks_needed = bytes_to_blocks(bytes_needed);
    uint32_t bitmap_size = blocks_to_bytes(blocks_needed);
    
    block_bitmap_buffer = malloc(bitmap_size);
    memset(block_bitmap_buffer, 0, bitmap_size);
    block_bitmap_blocks_to_track = blocks_to_track;
    block_bitmap_next_free_block = 0;
}

static inline bool bitmap_is_used(uint32_t block_no) { return block_bitmap_buffer[block_no / 8] & (1 << (block_no % 8)); }

uint32_t block_bitmap_allocate(int block_count) {
    while (bitmap_is_used(block_bitmap_next_free_block) && block_bitmap_next_free_block < block_bitmap_blocks_to_track)
        block_bitmap_next_free_block++;
    if (block_bitmap_next_free_block >= block_bitmap_blocks_to_track) fatal("Cannot allocate %ld blocks, all are exhausted");

    // we just hope the subsequent blocks are not used!
    uint32_t first_block = block_bitmap_next_free_block;
    block_bitmap_next_free_block += block_count;
    return first_block;
}

void block_bitmap_mark_used(uint32_t block_no) {
    block_bitmap_buffer[block_no / 8] |= (1 << (block_no % 8));
}

uint8_t *block_bitmap_get_bytes_ptr(int bitmap_block_no) {
    return block_bitmap_buffer + (bitmap_block_no * BLOCK_SIZE);
}

// -----------------------------------------------------------------------------

void import_host_file_into_img_sector(FILE *img, size_t sector_no, size_t sector_count, const char *host_file_path) {
    char *buffer = malloc(SECTOR_SIZE);

    // write to a sector directly
    FILE *f = fopen(host_file_path, "r");
    if (f == NULL) fatal("Error opening host file '%s'", host_file_path);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t sectors_needed = bytes_to_sectors(file_size);
    if (sectors_needed > sector_count)
        fatal("File '%s' with size %d cannot fit into %d sectors, needs %d sectors",
            host_file_path, file_size, sector_count, sectors_needed);

    for (int i = 0; i < sector_count; i++) {
        memset(buffer, 0, SECTOR_SIZE);
        fread(buffer, 1, SECTOR_SIZE, f);
        write_img_sector(img, sector_no + i, buffer);
    }

    free(buffer);
}

void persist_dir_entry(context *ctx, stored_inode *parent_dir, size_t rec_no, stored_dir_entry *entry) {
    size_t offset_in_file = rec_no * sizeof(stored_dir_entry);

    size_t block_index = offset_in_file / BLOCK_SIZE;
    if (block_index >= parent_dir->allocated_blocks)
        fatal("Cannot write entry %ld ('%s'-->%ld) to dir, requires block #%ld, but inode only has %d blocks", rec_no, entry->name, entry->inode_num, block_index, parent_dir->allocated_blocks);
    
    size_t abs_block_no = parent_dir->ranges[0].first_block_no + block_index;
    size_t offset_in_block = offset_in_file % BLOCK_SIZE;
    write_fs_block_part(ctx, abs_block_no, offset_in_block, entry, sizeof(stored_dir_entry));
}

void append_entry_to_directory(context *ctx, stored_inode *parent_dir, inode_no_t parent_dir_no, const char *entry_name, inode_no_t entry_no) {
    // printf("Adding entry '%s' to dir of inode %d, having a file size of %d\n", entry_name, parent_dir_no, parent_dir->file_size);
    
    stored_dir_entry entry;
    strncpy(entry.name, entry_name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_num = entry_no;
    
    size_t rec_no = parent_dir->file_size / sizeof(stored_dir_entry);
    persist_dir_entry(ctx, parent_dir, rec_no, &entry);
    
    parent_dir->file_size += sizeof(stored_dir_entry);
    if (parent_dir_no != ROOT_DIR_INODE_ID)
        persist_inode(ctx, parent_dir_no, parent_dir);
}

void persist_inode(context *ctx, inode_no_t inode_no, stored_inode *inode) {
    size_t offset_in_file = inode_no * sizeof(stored_inode);

    size_t block_index = offset_in_file / BLOCK_SIZE;
    size_t allocated = ctx->superblock.inodes_db_inode.allocated_blocks;
    if (block_index >= allocated)
        fatal("Cannot write inode %ld to db, requires block #%ld, but we have only %d blocks", inode_no, block_index, allocated);
    
    size_t abs_block_no = ctx->superblock.inodes_db_inode.ranges[0].first_block_no + block_index;
    size_t offset_in_block = offset_in_file % BLOCK_SIZE;
    write_fs_block_part(ctx, abs_block_no, offset_in_block, inode, sizeof(stored_inode));
}

