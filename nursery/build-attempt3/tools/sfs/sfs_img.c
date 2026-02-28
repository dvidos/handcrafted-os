#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include "../../kernel/filesys/fs_drivers/sfs/sfs_internal_persistent_structs.h"

#define DIV_CEIL(num, div)   (((num)+(div)-1)/(div))

// ---------------------------------------------------------

struct img {
    size_t total_size;
    size_t sector_size;
    FILE *handle;
    uint8_t *sector_buffer;
} img;

struct fs {
    size_t offset;
    stored_superblock superblock;
    uint32_t next_allocatable_block;
    uint8_t *bitmap;
    uint8_t *block_buffer;
} fs;

// ---------------------------------------------------------

static void fatal(const char *fmt, ...) {
    char buffer[256];

    va_list v;
    va_start(v, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, v);
    va_end(v);

    fprintf(stderr, "%s\n", buffer);
    exit(1);
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
int read_partition_entry(FILE *img, uint32_t *start_lba, uint32_t *sectors) {
    uint8_t entry[16];
    fseek(img, 0x1BE, SEEK_SET);
    if (fread(entry, 1, 16, img) != 16) fatal("Failed reading partition entry");
    *start_lba = *(uint32_t*)&entry[8];
    *sectors   = *(uint32_t*)&entry[12];
    return 0;
}
static FILE *create_img_file(const char *img_file, size_t img_size, size_t fs_offset) {
    FILE *f = fopen(img_file, "w+");
    if (f == NULL) fatal("Failed creating img file '%s'", img_file);
    int block_size = 64 * 1024;
    char *block = malloc(block_size);
    memset(block, 0, block_size);
    size_t remaining = img_size;
    while (remaining > 0) {
        size_t bytes = remaining < block_size ? remaining : block_size;
        fwrite(block, 1, bytes, f);
        remaining -= bytes;
    }
    write_bootable_mbr(f);
    write_partition_entry(f, DIV_CEIL(fs_offset, 512), DIV_CEIL(img_size - fs_offset, 512));
    return f;
}
static FILE *open_img_file(const char *img_file, size_t *img_size, size_t *fs_offset) {
    FILE *f = fopen(img_file, "r+");
    if (f == NULL) fatal("Failed opening img file '%s'", img_file);
    uint32_t start_lba = 0;
    uint32_t sectors = 0;
    read_partition_entry(f, &start_lba, &sectors);
    *img_size = start_lba * 512 + sectors * 512;
    *fs_offset = sectors * 512;
    return f;
}

static int allocate_fs_blocks(int count) {
    if (count == 0)
        return 0;
    
    if (fs.next_allocatable_block + count > fs.superblock.blocks_in_device)
        fatal("Cannot allocate %d blocks, all exhausted", count);

    int block = fs.next_allocatable_block;
    fs.next_allocatable_block += count;

    // also mark used in bitmap
    for (int i = 0; i < count; i++) {
        int byte = (block + i) / 8;
        int bit = (block + i) % 8;
        fs.bitmap[byte] |= (0x01 << bit);
    }

    return block;
}

static void initialize(const char *img_file, FILE *img_handle, size_t img_size, size_t fs_offset) {
    memset(&img, 0, sizeof(img));
    memset(&fs, 0, sizeof(fs));
    
    size_t block_size = 1024; // can be 512, 1K, 2K, 4K...
    size_t sector_size = 512;
    size_t fs_bytes = img_size - fs_offset;
    size_t bitmap_size_in_bits = (fs_bytes + (block_size - 1)) / block_size;
    size_t bitmap_size_in_bytes = (bitmap_size_in_bits + 7) / 8;
    size_t bitmap_size_in_blocks = (bitmap_size_in_bytes + block_size - 1) / block_size;

    img.total_size = img_size;
    img.sector_size = sector_size;
    img.handle = img_handle;
    img.sector_buffer = malloc(sector_size);
    
    fs.offset = fs_offset;
    memcpy(fs.superblock.magic, "SFS1", 4);
    fs.superblock.block_size_in_bytes = block_size;
    fs.superblock.blocks_in_device = (fs_bytes + (block_size - 1)) / block_size;
    fs.superblock.blocks_bitmap_first_block  = 1;
    fs.superblock.blocks_bitmap_blocks_count = bitmap_size_in_blocks;
    fs.superblock.direntry_size = sizeof(stored_dir_entry);
    fs.superblock.inode_size = sizeof(stored_inode);
    fs.superblock.sector_size = sector_size;
    fs.superblock.sectors_per_block = block_size / sector_size;
    strcpy(fs.superblock.volume_label, "HCOS");
    fs.bitmap = malloc(bitmap_size_in_bytes);
    memset(fs.bitmap, 0, bitmap_size_in_bytes);
    fs.next_allocatable_block = 0;
    fs.block_buffer = malloc(block_size);

    allocate_fs_blocks(1); // superblock;
    allocate_fs_blocks(fs.superblock.blocks_bitmap_blocks_count); // bitmap
}

// ---------------------------------------------------------

static void import_into_sector(size_t sector_no, size_t sector_count, const char *host_file_path) {
    // write to a sector directly
    FILE *f = fopen(host_file_path, "r");
    if (f == NULL) fatal("Error opening host file '%s'", host_file_path);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t sectors_needed = (file_size + img.sector_size - 1) / img.sector_size;
    if (sectors_needed > sector_count)
        fatal("File '%s' with size %d cannot fit into %d sectors, needs %d sectors",
            host_file_path, file_size, sector_count, sectors_needed);

    for (int i = 0; i < sector_count; i++) {
        memset(img.sector_buffer, 0, img.sector_size);
        size_t read = fread(img.sector_buffer, 1, img.sector_size, f);

        int offset = (sector_no + i) * img.sector_size;
        fseek(img.handle, offset, SEEK_SET);
        fwrite(img.sector_buffer, 1, img.sector_size, img.handle);

        if (read == 0) break;
    }
}

static int count_entries_in_host_dir(const char *host_dir_path) {
    // count entries (. and .. too) in order to allocate blocks
    DIR *dir = opendir(host_dir_path);
    if (dir == NULL) fatal("error opening dir '%s'", host_dir_path);

    struct dirent *e;
    int count = 0;
    while ((e = readdir(dir)) != NULL)
        count++;
    closedir(dir);
    return count;
}

static stored_inode create_new_inode(bool is_file, size_t file_size, inode_no_t *inode_no) {
    stored_inode n;
    memset(&n, 0, sizeof(stored_inode));

    // allocate as many blocks as needed
    int blocks_needed = (file_size + fs.superblock.block_size_in_bytes - 1) / fs.superblock.block_size_in_bytes;
    int first_block = allocate_fs_blocks(blocks_needed);

    n.type_perms = is_file ? STORED_INODE_TYPE_FILE : STORED_INODE_TYPE_DIR;
    n.user_id = 0;
    n.group_id = 0;
    n.file_size = file_size;
    n.allocated_blocks = blocks_needed;
    n.padding = 0;
    n.created_at = time(NULL);
    n.modified_at = time(NULL);
    n.ranges[0].first_block_no = first_block;
    n.ranges[0].blocks_count = blocks_needed;
    n.indirect_ranges_block_no = 0;
    n.double_indirect_block_no = 0;
    
    // grab inode num, and increase
    *inode_no = fs.superblock.inodes_db_rec_count;
    fs.superblock.inodes_db_rec_count += 1;

    return n;
}

static void write_in_fs_block_in_image(int abs_block_no, int offset_in_block, const void *buffer, int size) {
    long offset = fs.offset + (abs_block_no * fs.superblock.block_size_in_bytes) + offset_in_block;
    fseek(img.handle, offset, SEEK_SET);
    size_t written = fwrite(buffer, 1, size, img.handle);
    if (written != size) fatal("Intended to write %d bytes, but wrote %u", size, written);
}

static void persist_inodes_record(inode_no_t inode_no, stored_inode *inode) {
    int offset_in_file = inode_no * sizeof(stored_inode);
    int block_index = offset_in_file / fs.superblock.block_size_in_bytes;
    
    if (block_index >= fs.superblock.inodes_db_inode.allocated_blocks)
    fatal("Wanting to write inode %d, but inodes_db only has %d blocks", inode_no, fs.superblock.inodes_db_inode.allocated_blocks);
    int block_no = fs.superblock.inodes_db_inode.ranges[0].first_block_no + block_index;
    
    int offset_in_block = offset_in_file % fs.superblock.block_size_in_bytes;
    write_in_fs_block_in_image(block_no, offset_in_block, inode, sizeof(stored_inode));
}

static void append_dir_entry(stored_inode *dir_inode, const char *name, inode_no_t inode_no) {
    int bytes_in_file = dir_inode->file_size;
    int recs_in_file = bytes_in_file / sizeof(stored_dir_entry);
    int offset_in_file = recs_in_file * sizeof(stored_dir_entry);
    int block_index     = offset_in_file / fs.superblock.block_size_in_bytes;
    if (block_index >= dir_inode->ranges[0].blocks_count)
        fatal("Cannot add entry '%s' to dir, it has %d blocks and %d entries", name, dir_inode->ranges[0].blocks_count, recs_in_file);

    int block_no        = dir_inode->ranges[0].first_block_no + block_index;
    int offset_in_block = offset_in_file % fs.superblock.block_size_in_bytes;
    stored_dir_entry entry;
    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_num = inode_no;

    write_in_fs_block_in_image(block_no, offset_in_block, &entry, sizeof(stored_dir_entry));
    dir_inode->file_size += sizeof(stored_dir_entry);
}

int create_and_write_file(stored_inode *parent_dir_inode, const char *file_name, const char *host_file_path) {
    FILE *f = fopen(host_file_path, "r");
    if (f == NULL) fatal("Error opening host file '%s'", host_file_path);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    inode_no_t inode_no;
    stored_inode file_inode = create_new_inode(true, (size_t)file_size, &inode_no);

    int block_no = file_inode.ranges[0].first_block_no;
    while (file_size > 0) {
        size_t read = fread(fs.block_buffer, 1, fs.superblock.block_size_in_bytes, f);
        write_in_fs_block_in_image(block_no, 0, fs.block_buffer, read);
        block_no++;
        file_size -= read;
    }
    
    persist_inodes_record(inode_no, &file_inode);
    append_dir_entry(parent_dir_inode, file_name, inode_no);
}

int create_directory(inode_no_t parent_inode_no, stored_inode *parent_dir_inode, const char *dir_name, int expected_entries) {
    // somehow pass in the parent dir inode, and expected entries
    // allocate blocks as needed for all entries
    long file_size = expected_entries * sizeof(stored_dir_entry);

    inode_no_t inode_no;
    stored_inode dir_inode = create_new_inode(false, (size_t)file_size, &inode_no);
    dir_inode.file_size = 0;
    persist_inodes_record(inode_no, &dir_inode);

    append_dir_entry(&dir_inode, ".", inode_no);
    append_dir_entry(&dir_inode, "..", parent_inode_no);

    append_dir_entry(parent_dir_inode, dir_name, inode_no);
}

// ---------------------------------------------------------

int import_file_from_host(const char *host_file_path, const char *sfs_file_path) {
    // find dir inode, 
}

int import_dir_contents_from_host(const char *host_dir_path, const char *sfs_dir_path) {
    // find dir inode, then loop

}

// ---------------------------------------------------------

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

// ---------------------------------------------------------

void do_help_and_exit() {
    printf("Syntax:\n");
    printf("     sfs_img <command> [args]\n");
    printf("Commands: \n");
    printf("     sfs_img create-img <image_file> <size_bytes> <fs_offset>\n");
    printf("     sfs_img write-sector <image_file> <sector_no> <sector_count> <host_file>\n");
    printf("     sfs_img import-file <image_file> <host_file> <sfs_path>\n");
    printf("     sfs_img import-dir <image_file> <host_dir> <sfs_dir>\n");
    printf("\n");
    exit(1);
}

void do_create_img(int argc, char *argv[]) {
    if (argc < 3) do_help_and_exit();
    const char *image_file = argv[0];
    long size_bytes = parse_size(argv[1]);
    long fs_offset = parse_size(argv[2]);

    FILE *f = create_img_file(image_file, size_bytes, fs_offset);
    initialize(image_file, f, size_bytes, fs_offset);

    printf("Image file '%s' created\n", image_file);
}

void do_write_sector(int argc, char *argv[]) {
    if (argc < 4) do_help_and_exit();
    const char *image_file = argv[0];
    long sector_no = atol(argv[1]);
    long sector_count = atol(argv[2]);
    const char *host_file = argv[3];

    size_t img_size, fs_offset;
    FILE *f = open_img_file(image_file, &img_size, &fs_offset);
    initialize(image_file, f, img_size, fs_offset);
    
    if ((sector_no + sector_count) * 512 >= fs_offset)
        fatal("Writing %ld sectors at sector %ld would overwrite FS partition. Can fit at most %ld sectors", sector_count, sector_no, (fs_offset / 512) - sector_no);

    import_into_sector(sector_no, sector_count, host_file);
    if (sector_no == 0) {
        write_partition_entry(f, DIV_CEIL(fs_offset, 512), DIV_CEIL(img_size - fs_offset, 512));
        write_bootable_mbr(f);
    }

    printf("Wrote file '%s' at sector %ld\n", host_file, sector_no);
}

void do_import_file(int argc, char *argv[]) {
    if (argc < 3) do_help_and_exit();
    const char *image_file = argv[0];
    const char *host_file = argv[1];
    const char *sfs_path = argv[2];

    size_t img_size, fs_offset;
    FILE *f = open_img_file(image_file, &img_size, &fs_offset);
    initialize(image_file, f, img_size, fs_offset);

    printf("Imported file '%s' into %s\n", host_file, sfs_path);
}

void do_import_dir(int argc, char *argv[]) {
    if (argc < 3) do_help_and_exit();
    const char *image_file = argv[0];
    const char *host_dir = argv[1];
    const char *sfs_dir = argv[2];

    size_t img_size, fs_offset;
    FILE *f = open_img_file(image_file, &img_size, &fs_offset);
    initialize(image_file, f, img_size, fs_offset);

    printf("Imported dir '%s' into %s\n", host_dir, sfs_dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) do_help_and_exit();

    char *cmd = argv[1];
    if      (strcmp(cmd, "create-img")   == 0) do_create_img  (argc - 2, argv + 2);
    else if (strcmp(cmd, "write-sector") == 0) do_write_sector(argc - 2, argv + 2);
    else if (strcmp(cmd, "import-file")  == 0) do_import_file (argc - 2, argv + 2);
    else if (strcmp(cmd, "import-dir")   == 0) do_import_dir  (argc - 2, argv + 2);
    else do_help_and_exit();
}
