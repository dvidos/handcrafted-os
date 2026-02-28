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
#include "functions.h"


// ---------------------------------------------------------

#define EXPECTED_ROOT_DIR_ENTRIES     32
#define EXPECTED_INODES              128

// ---------------------------------------------------------

static void initialize_by_creating(const char *img_file, size_t img_size, size_t fs_offset, context *ctx) {
    char *block_buffer = malloc(BLOCK_SIZE);

    create_host_file(img_file, img_size);
    ctx->img = open_host_file(img_file);
    write_bootable_mbr(ctx->img);
    write_partition_entry(ctx->img, bytes_to_sectors(fs_offset), bytes_to_sectors(img_size - fs_offset));
    ctx->img_size = img_size;
    ctx->fs_offset = fs_offset;

    // prepare superblock
    memset(&ctx->superblock, 0, sizeof(stored_superblock));
    memcpy(ctx->superblock.magic, "SFS1", 4);
    ctx->superblock.block_size_in_bytes = BLOCK_SIZE;
    ctx->superblock.blocks_in_device = bytes_to_blocks(img_size - fs_offset);
    ctx->superblock.blocks_bitmap_first_block  = 1;
    ctx->superblock.blocks_bitmap_blocks_count = bits_to_blocks(ctx->superblock.blocks_in_device);
    ctx->superblock.direntry_size = sizeof(stored_dir_entry);
    ctx->superblock.inode_size = sizeof(stored_inode);
    ctx->superblock.sector_size = SECTOR_SIZE;
    ctx->superblock.sectors_per_block = BLOCK_SIZE / SECTOR_SIZE;
    strcpy(ctx->superblock.volume_label, "HCOS");

    // prepare bitmap
    block_bitmap_create(ctx->superblock.blocks_in_device);
    block_bitmap_mark_used(0);
    for (uint32_t i = 0; i < ctx->superblock.blocks_bitmap_blocks_count; i++)
        block_bitmap_mark_used(ctx->superblock.blocks_bitmap_first_block + i);

    // we now need to allocate for the inodes and the root dir.
    uint32_t block_count = bytes_to_blocks(EXPECTED_INODES * sizeof(stored_inode));
    uint32_t block_no = block_bitmap_allocate(block_count);
    ctx->superblock.inodes_db_inode.allocated_blocks = block_count;
    ctx->superblock.inodes_db_inode.ranges[0].first_block_no = block_no;
    ctx->superblock.inodes_db_inode.ranges[0].blocks_count = block_count;

    block_count = bytes_to_blocks(EXPECTED_INODES * sizeof(stored_dir_entry));
    block_no = block_bitmap_allocate(block_count);
    ctx->superblock.root_dir_inode.allocated_blocks = block_count;
    ctx->superblock.root_dir_inode.ranges[0].first_block_no = block_no;
    ctx->superblock.root_dir_inode.ranges[0].blocks_count = block_count;

    // save FS data
    memcpy(block_buffer, &ctx->superblock, sizeof(stored_superblock));
    write_fs_block(ctx, 0, block_buffer);
    for (uint32_t i = 0; i < ctx->superblock.blocks_bitmap_blocks_count; i++)
        write_fs_block(ctx, ctx->superblock.blocks_bitmap_first_block + i, block_bitmap_get_bytes_ptr(i));

    free(block_buffer);
}

static void initialize_by_opening(const char *img_file, context *ctx) {
    char *block_buffer = malloc(BLOCK_SIZE);

    ctx->img = open_host_file(img_file);
    uint32_t part_start_lba, part_sectors;
    read_partition_entry(ctx->img, &part_start_lba, &part_sectors);
    ctx->img_size = sectors_to_bytes(part_start_lba + part_sectors);
    ctx->fs_offset = sectors_to_bytes(part_start_lba);

    // load FS data
    read_fs_block(ctx, 0, block_buffer);
    memcpy(&ctx->superblock, block_buffer, sizeof(stored_superblock));

    if (memcmp(ctx->superblock.magic, "SFS1", 4) != 0) fatal("Superblock bad magic: 0x%02x 0x%02x 0x%02x 0x%02x\n", ctx->superblock.magic[0], ctx->superblock.magic[1], ctx->superblock.magic[2], ctx->superblock.magic[3]);
    if (ctx->superblock.block_size_in_bytes != BLOCK_SIZE) fatal("Superblock bad block size");
    if (ctx->superblock.sector_size != SECTOR_SIZE) fatal("Superblock bad sector size");
    if (ctx->superblock.inode_size != sizeof(stored_inode)) fatal("Superblock bad inode size");
    if (ctx->superblock.direntry_size != sizeof(stored_dir_entry)) fatal("Superblock bad direntry size");

    // we can trust it. init and load bitmap
    block_bitmap_create(ctx->superblock.blocks_in_device);
    for (uint32_t i = 0; i < ctx->superblock.blocks_bitmap_blocks_count; i++)
        read_fs_block(ctx, ctx->superblock.blocks_bitmap_first_block + i, block_bitmap_get_bytes_ptr(i));

    free(block_buffer);
}

// ---------------------------------------------------------

static stored_inode create_new_inode(stored_superblock *superblock, bool is_file, size_t file_size, inode_no_t *inode_no) {
    stored_inode n;
    memset(&n, 0, sizeof(stored_inode));

    // allocate as many blocks as needed
    int blocks_needed = bytes_to_blocks(file_size);
    int first_block = block_bitmap_allocate(blocks_needed);

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
    *inode_no = superblock->inodes_db_rec_count;
    superblock->inodes_db_rec_count += 1;

    return n;
}

// ---------------------------------------------------------

int import_file_from_host(context *ctx, const char *host_file, stored_inode *file_inode, inode_no_t file_no) {
    char *block_buffer;

    FILE *f = fopen(host_file, "r");
    if (f == NULL) fatal("Cannot open '%s' for reading", host_file);
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // we assume inode has already allocated enough bytes
    size_t blocks_needed = bytes_to_blocks(file_size);
    if (file_inode->ranges[0].blocks_count < blocks_needed)
        fatal("File '%s' would need %d blocks allocated, but only %d were found", host_file, blocks_needed, file_inode->ranges[0].blocks_count);
    
    block_buffer = malloc(BLOCK_SIZE);
    int block_no = file_inode->ranges[0].first_block_no;
    while (file_size > 0) {
        size_t read = fread(block_buffer, 1, BLOCK_SIZE, f);
        write_fs_block(ctx, block_no, block_buffer);
        block_no++;
        file_size -= read;
        file_inode->file_size += read;
    }

    free(block_buffer);
    fclose(f);

    persist_inode(ctx, file_no, file_inode);
}

void import_dir_contents_recursively(context *ctx, const char *host_dir, stored_inode *parent_dir_inode, inode_no_t parent_dir_no) {
    char *entry_path;

    printf("Importing from dir %s...\n", host_dir);

    entry_path = malloc(1024);
    entry_path[0] = 0;

    DIR *d = opendir(host_dir);
    if (d == NULL) fatal("Could not open dir '%s' on host", host_dir);

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        strcpy(entry_path, host_dir); strcat(entry_path, "/"); strcat(entry_path, entry->d_name);
        inode_no_t new_inode_no;
        stored_inode new_inode;

        if (S_ISDIR(entry->d_type)) {

            size_t num_entries = count_entries_in_host_dir(entry_path);
            new_inode = create_new_inode(&ctx->superblock, false, num_entries * sizeof(stored_dir_entry), &new_inode_no);
            import_dir_contents_recursively(ctx, entry_path, &new_inode, new_inode_no);
            persist_inode(ctx, new_inode_no, &new_inode);
            
            append_entry_to_directory(ctx, parent_dir_inode, parent_dir_no, entry->d_name, new_inode_no);

        } else if (S_ISREG(entry->d_type)) {

            size_t file_size = get_host_file_size(entry_path);
            new_inode = create_new_inode(&ctx->superblock, true, file_size, &new_inode_no);
            import_file_from_host(ctx, entry_path, &new_inode, new_inode_no);
            persist_inode(ctx, new_inode_no, &new_inode);
            
            append_entry_to_directory(ctx, parent_dir_inode, parent_dir_no, entry->d_name, new_inode_no);
        }
    }

    closedir(d);
    free(entry_path);
}


// ---------------------------------------------------------

void do_help_and_exit() {
    printf("Syntax:\n");
    printf("     sfs_img <command> [args]\n");
    printf("Commands: \n");
    printf("     sfs_img create-img <image_file> <size_bytes> <fs_offset>\n");
    printf("     sfs_img write-sector <image_file> <sector_no> <sector_count> <host_file>\n");
    printf("     sfs_img import-dir <image_file> <host_dir> <sfs_dir>\n");
    printf("\n");
    exit(1);
}

void do_create_img(int argc, char *argv[]) {
    if (argc < 3) do_help_and_exit();
    const char *image_file = argv[0];
    long size_bytes = parse_size(argv[1]);
    long fs_offset = parse_size(argv[2]);

    context ctx;
    initialize_by_creating(image_file, size_bytes, fs_offset, &ctx);
    fclose(ctx.img);

    printf("Image file '%s' created\n", image_file);
}

void do_write_sector(int argc, char *argv[]) {
    if (argc < 4) do_help_and_exit();
    const char *image_file = argv[0];
    long sector_no = atol(argv[1]);
    long sector_count = atol(argv[2]);
    const char *host_file = argv[3];

    context ctx;
    initialize_by_opening(image_file, &ctx);
    
    if (sectors_to_bytes(sector_no + sector_count) >= ctx.fs_offset)
        fatal("Writing %ld sectors at sector %ld would overwrite FS partition. Can fit at most %ld sectors", sector_count, sector_no, bytes_to_sectors(ctx.fs_offset) - sector_no);

    import_host_file_into_img_sector(ctx.img, sector_no, sector_count, host_file);
    if (sector_no == 0) {
        // these would have been overwritten
        write_partition_entry(ctx.img, bytes_to_sectors(ctx.fs_offset), bytes_to_sectors(ctx.img_size - ctx.fs_offset));
        write_bootable_mbr(ctx.img);
    }

    fclose(ctx.img);
    printf("Wrote file '%s' at sector %ld\n", host_file, sector_no);
}

void do_import_dir_contents(int argc, char *argv[]) {
    if (argc < 3) do_help_and_exit();
    const char *image_file = argv[0];
    const char *host_dir = argv[1];
    const char *sfs_dir = argv[2];

    context ctx;
    initialize_by_opening(image_file, &ctx);

    import_dir_contents_recursively(&ctx, host_dir, &ctx.superblock.root_dir_inode, ROOT_DIR_INODE_ID);

    // should write superblock & bitmaps and others.
    
    printf("Imported dir '%s' into %s\n", host_dir, sfs_dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) do_help_and_exit();

    char *cmd = argv[1];
    if      (strcmp(cmd, "create-img")   == 0) do_create_img         (argc - 2, argv + 2);
    else if (strcmp(cmd, "write-sector") == 0) do_write_sector       (argc - 2, argv + 2);
    else if (strcmp(cmd, "import-dir")   == 0) do_import_dir_contents(argc - 2, argv + 2);
    else do_help_and_exit();
}
