#include "../../../../config.inc.h"
#define ENABLE_UNIT_TESTS
#ifdef ENABLE_UNIT_TESTS




// sfs_tests.c

#define TEST_DISK_SIZE_BYTES        (4 * 1024 * 1024)                            // 4 MB disk
#define TEST_DISK_BLOCK_SIZE        512                                         // 512 bytes per block
#define TEST_FS_BLOCK_SIZE          1024                                         // for disks with 2MB..8MB capacity
#define TEST_FS_NUM_BLOCKS          (TEST_DISK_SIZE_BYTES / TEST_FS_BLOCK_SIZE)  // 4k, needing 512 bytes, but we store 1k blocks
#define TEST_FS_NUM_INODES          256
#define TEST_INODES_NUM_BLOCKS      ((TEST_FS_NUM_INODES * sizeof(stored_inode))/TEST_FS_BLOCK_SIZE)
#define TEST_FILENAME "test_file.txt"
#define TEST_DATA_SHORT "Hello, SFS!"
#define TEST_DATA_LONG "This is a longer string to test multi-block writes and reads in the Simple File System. It should span across multiple blocks to properly exercise the block allocation and retrieval mechanisms. This string needs to be significantly longer than a single block to ensure the test is effective. We are aiming for at least two blocks worth of data, so let's make it even longer to be safe."
#define TEST_DIR_NAME "test_dir"


// Include necessary VFS and SFS headers.
#include "../../../devices/block/block_device.h"
#include "../../fs_driver.h"
#include "../../vfs_api.h"
#include "sfs.h"
#include "../../fs_drivers/sfs/sfs_internal.h" // For SFS internal structures and function declarations

// Mocked Headers / Globals (if needed and not provided by included files)
#include "../../../klib/path.h"
#include "../../../klib/string.h"
#include "../../../memory/kheap.h"
#include "../../../logger/logger.h"
#include "../../../utils/assert.h" // Provides assert
#include "../../../klib/backed_cache.h" // For cache API

#define assert(x) ASSERT(x)


// --- Mock Block Device Implementation ---

typedef struct {
    block_device_t base; // Embed the base struct
    void *memory;        // Pointer to the allocated memory buffer
    size_t size_bytes;   // Total size of the memory buffer
    size_t block_size;   // Size of each block
    size_t block_count;  // Total number of blocks
} mock_block_device_t;

static error_t mock_dev_read(struct block_device *bdev, uint64_t lba, size_t n_lbas, void *buf);
static error_t mock_dev_write(struct block_device *bdev, uint64_t lba, size_t n_lbas, const void *buf);
static error_t mock_dev_ioctl(struct block_device *bdev, uint32_t cmd, long arg);

static const struct block_device_ops mock_dev_ops = {
    .read_sectors = mock_dev_read,
    .write_sectors = mock_dev_write
};

static mock_block_device_t* create_mock_device(size_t total_size_bytes, size_t block_size) {
    if (block_size == 0 || total_size_bytes == 0) return NULL;
    size_t block_count = (total_size_bytes + block_size - 1) / block_size;

    mock_block_device_t *mock_bdev = kmalloc(sizeof(mock_block_device_t));
    assert(mock_bdev != NULL);

    mock_bdev->memory = kmalloc(total_size_bytes);
    assert(mock_bdev->memory != NULL);
    memset(mock_bdev->memory, 0, total_size_bytes);

    mock_bdev->size_bytes = total_size_bytes;
    mock_bdev->block_size = block_size;
    mock_bdev->block_count = block_count;

    mock_bdev->base.block_size = block_size;
    mock_bdev->base.total_blocks = block_count;
    mock_bdev->base.ops = (struct block_device_ops *)&mock_dev_ops;

    return mock_bdev;
}

static void destroy_mock_device(mock_block_device_t *mock_bdev) {
    if (!mock_bdev) return;
    kfree(mock_bdev->memory);
    kfree(mock_bdev);
}

static error_t mock_dev_read(struct block_device *bdev, uint64_t lba, size_t n_lbas, void *buf) {
    mock_block_device_t *mock_bdev = container_of(bdev, mock_block_device_t, base);
    size_t offset = lba * mock_bdev->block_size;
    size_t bytes_to_read = n_lbas * mock_bdev->block_size;

    if (lba >= mock_bdev->block_count) return ERR_INVALID_ARGS;
    if (offset >= mock_bdev->size_bytes) return ERR_INVALID_ARGS;

    if (offset + bytes_to_read > mock_bdev->size_bytes) {
        bytes_to_read = mock_bdev->size_bytes - offset;
    }

    memcpy(buf, (char*)mock_bdev->memory + offset, bytes_to_read);
    return OK;
}

static error_t mock_dev_write(struct block_device *bdev, uint64_t lba, size_t n_lbas, const void *buf) {
    mock_block_device_t *mock_bdev = container_of(bdev, mock_block_device_t, base);
    size_t offset = lba * mock_bdev->block_size;
    size_t bytes_to_write = n_lbas * mock_bdev->block_size;

    if (lba >= mock_bdev->block_count) return ERR_INVALID_ARGS;
    if (offset >= mock_bdev->size_bytes) return ERR_INVALID_ARGS;

    if (offset + bytes_to_write > mock_bdev->size_bytes) {
        bytes_to_write = mock_bdev->size_bytes - offset;
    }

    memcpy((char*)mock_bdev->memory + offset, buf, bytes_to_write);
    return OK;
}

// --- Global Context Setup for Tests ---
// Assumes global objects like 'inodes', 'mtab', 'open_files' from vfs_impl.c are visible.

static vfs_context_t* create_vfs_context() {
    vfs_context_t *ctx = kmalloc(sizeof(vfs_context_t));
    assert(ctx != NULL);
    ctx->mtab = create_mount_table();
    ctx->uid = 0;
    ctx->gid = 0;
    ctx->creation_mask = 0022;
    ctx->root_inode = inodes.empty();
    ctx->cwd_inode = inodes.empty();
    return ctx;
}

static void cleanup(open_file_t *file, vfs_context_t *ctx, mock_block_device_t *mock_bdev) {
    error_t err;
    if (file) {
        err = vfs_close(file);
        assert(err == OK);
    }
    if (ctx) {
        if (!inodes.is_empty(&ctx->root_inode)) {
            err = vfs_unmount(ctx, "/");
            assert(err == OK);
        }
        ctx->mtab->ops->destroy(ctx->mtab);
        kfree(ctx);
    }
    if (mock_bdev) {
        destroy_mock_device(mock_bdev);
    }
}

// --- Test Cases ---

static void test_sfs_mkfs_success() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    // Use simple_fs.mkfs directly as provided by sfs.c
    err = simple_fs.mkfs(dev);
    assert(err == OK);
    
    stored_superblock sb_read_back;
    err = mock_dev_read(dev, 0, 1, &sb_read_back);
    assert(err == OK);
    // log_debug_hex(&sb_read_back, sizeof(stored_superblock), 0);

    assert(memcmp(sb_read_back.magic, "SFS1", 4) == 0);
    assert(sb_read_back.direntry_size == sizeof(stored_dir_entry));
    assert(sb_read_back.inode_size == sizeof(stored_inode));
    assert(sb_read_back.block_size_in_bytes >= 512 && sb_read_back.block_size_in_bytes <= 4096);
    assert(sb_read_back.sector_size >= 512 && sb_read_back.sector_size <= 4096);

    destroy_mock_device(mock_bdev);
}

static void test_sfs_mount_success() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    err = simple_fs.mkfs(dev);
    assert(err == OK);
    
    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    assert(!inodes.is_empty(&ctx->root_inode));
    assert(!inodes.is_empty(&ctx->cwd_inode));
    assert(ctx->root_inode.sb != NULL);
    assert(ctx->root_inode.sb->driver == simple_fs.ops);

    cleanup(NULL, ctx, mock_bdev);
}

static void test_sfs_create_file_basic() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    inode_t target_inode;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);

    err = vfs_lookup(ctx, TEST_FILENAME, &target_inode);
    assert(err == OK);
    assert(!inodes.is_empty(&target_inode));
    assert(!inodes.is_dir(&target_inode));
    assert(target_inode.sb != NULL && target_inode.sb == ctx->root_inode.sb);

    cleanup(NULL, ctx, mock_bdev);
}

static void test_sfs_create_write_read_close_basic() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;

    const char *filename = TEST_FILENAME;
    const char *data_to_write = TEST_DATA_SHORT;
    const size_t data_len = strlen(data_to_write);

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);

    int open_flags = O_WRONLY | O_CREAT | O_TRUNC;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);
    assert(file->offset == 0);
    assert(file->size == 0);

    ssize_t bytes_written = vfs_write(file, data_to_write, data_len);
    assert(bytes_written == (ssize_t)data_len);
    assert(file->offset == data_len);
    assert(file->size == data_len);

    err = vfs_close(file); file = NULL;
    assert(err == OK);

    open_flags = O_RDONLY;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);
    assert(file->offset == 0);
    assert(file->size == data_len);

    char read_buffer[128] = {0};
    ssize_t bytes_read = vfs_read(file, read_buffer, sizeof(read_buffer) - 1);
    assert(bytes_read == (ssize_t)data_len);
    read_buffer[bytes_read] = '\0';

    assert(strcmp(read_buffer, data_to_write) == 0);
    assert(file->offset == data_len);

    err = vfs_close(file); file = NULL;
    assert(err == OK);

    cleanup(file, ctx, mock_bdev);
}

static void test_sfs_seek_and_offsets() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;

    const char *filename = TEST_FILENAME;
    const char *data_initial = "Hello ";          // len = 6
    const char *data_overwrite = "World!";        // len = 6
    const char *data_append = " Appended";       // len = 9
    const char *data_sparse_start = "Sparse!";   // len = 7
    const size_t initial_len = strlen(data_initial);
    const size_t overwrite_len = strlen(data_overwrite);
    const size_t append_len = strlen(data_append);
    const size_t sparse_len = strlen(data_sparse_start);

    // Buffer large enough for all test data, including holes and zeros
    char read_buffer[TEST_DISK_BLOCK_SIZE * 2]; 
    char expected_read_buffer[TEST_DISK_BLOCK_SIZE * 2];
    memset(expected_read_buffer, 0, sizeof(expected_read_buffer)); // Initialize to all zeros

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);

    int open_flags = O_RDWR; // Open for read/write
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);
    assert(file->offset == 0);
    assert(file->size == 0); // Initially empty file

    // --- 1. Sequential Write ---
    // Write "Hello "
    ssize_t bytes_written = vfs_write(file, data_initial, initial_len);
    assert(bytes_written == (ssize_t)initial_len);
    assert(file->offset == initial_len);
    assert(file->size == initial_len);
    memcpy(expected_read_buffer, data_initial, initial_len);

    // --- 2. Seek SET and Overwrite ---
    // Seek to beginning, write "World!". Should overwrite "Hello "
    off_t seek_pos = vfs_seek(file, 0, SEEK_SET);
    assert(seek_pos == 0);
    assert(file->offset == 0);

    bytes_written = vfs_write(file, data_overwrite, overwrite_len);
    assert(bytes_written == (ssize_t)overwrite_len);
    assert(file->offset == overwrite_len);
    assert(file->size == initial_len); // File size should remain initial_len (6) as "World!" also has length 6
    memcpy(expected_read_buffer, data_overwrite, overwrite_len); // Update expected content

    // --- 3. Seek END and Append ---
    // Seek to current end, write " Appended"
    seek_pos = vfs_seek(file, 0, SEEK_END);
    assert(seek_pos == (off_t)initial_len); // Current file size is initial_len (6)
    assert(file->offset == initial_len);

    bytes_written = vfs_write(file, data_append, append_len);
    assert(bytes_written == (ssize_t)append_len);
    assert(file->offset == initial_len + append_len);
    assert(file->size == initial_len + append_len); // New size is 6 + 9 = 15
    memcpy(expected_read_buffer + initial_len, data_append, append_len); // Update expected content

    // Current file content: "World! Appended" (total 15 bytes)

    // --- 4. Seek CUR past EOF and Write (Sparse File) ---
    // Seek 10 bytes from current position (15). New offset = 15 + 10 = 25.
    // This creates a 10-byte hole. Then write "Sparse!"
    size_t hole_size = 10;
    off_t seek_target_sparse = file->offset + hole_size; // 15 + 10 = 25
    seek_pos = vfs_seek(file, hole_size, SEEK_CUR);
    assert(seek_pos == seek_target_sparse);
    assert((off_t)file->offset == seek_target_sparse);

    bytes_written = vfs_write(file, data_sparse_start, sparse_len);
    assert(bytes_written == (ssize_t)sparse_len);
    assert(file->offset == seek_target_sparse + sparse_len); // 25 + 7 = 32
    assert(file->size == seek_target_sparse + sparse_len); // File size is now 32, covering the hole
    memcpy(expected_read_buffer + seek_target_sparse, data_sparse_start, sparse_len); // Update expected content

    // --- 5. Read and Verify Content including Hole ---
    // Expected content: "World! Appended" + 10 NULs + "Sparse!" (total 32 bytes)
    err = vfs_close(file); file = NULL; // Close and re-open to ensure disk-backed read
    assert(err == OK);

    open_flags = O_RDONLY;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);
    assert(file->offset == 0);
    assert(file->size == (uint64_t)(seek_target_sparse + sparse_len)); // File size is 32

    memset(read_buffer, 0xFF, sizeof(read_buffer)); // Fill with non-zero to detect actual zeros
    ssize_t bytes_read = vfs_read(file, read_buffer, file->size); // Read exactly file->size
    assert(bytes_read == (ssize_t)file->size);
    
    assert(memcmp(read_buffer, expected_read_buffer, file->size) == 0); // Verify all content

    // --- 6. Seek to specific position and Read ---
    // Seek to the start of "Appended"
    seek_pos = vfs_seek(file, initial_len, SEEK_SET); // Seek to offset 6
    assert(seek_pos == (off_t)initial_len);
    assert(file->offset == initial_len);

    memset(read_buffer, 0, sizeof(read_buffer));
    bytes_read = vfs_read(file, read_buffer, append_len); // Read " Appended"
    assert(bytes_read == (ssize_t)append_len);
    assert(memcmp(read_buffer, data_append, append_len) == 0);
    assert(file->offset == initial_len + append_len);


    // --- 7. Seek negative from END ---
    // Test seeking a small negative offset from END
    off_t neg_seek_amount = 5;
    seek_pos = vfs_seek(file, -neg_seek_amount, SEEK_END);
    assert(seek_pos == (off_t)(file->size - neg_seek_amount));
    assert(file->offset == (uint64_t)(file->size - neg_seek_amount));

    // Test seeking very negative from END (should clamp to 0)
    // Create a scenario where offset becomes negative
    off_t huge_neg_seek_amount = (off_t)(file->size + 100);
    seek_pos = vfs_seek(file, -huge_neg_seek_amount, SEEK_END); // Seek past beginning
    assert(seek_pos == 0);
    assert(file->offset == 0);


    cleanup(file, ctx, mock_bdev);
}

static void test_sfs_write_multiblock_read_verify() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;

    const char *filename = TEST_FILENAME;
    const char *data_to_write = TEST_DATA_LONG; 
    const size_t data_len = strlen(data_to_write);

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE); 
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);

    int open_flags = O_RDWR | O_CREAT | O_TRUNC;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);

    ssize_t bytes_written = vfs_write(file, data_to_write, data_len);
    assert(bytes_written == (ssize_t)data_len);
    assert(file->size == data_len);

    err = vfs_close(file); file = NULL;
    assert(err == OK);

    open_flags = O_RDONLY;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    assert(file != NULL);
    assert(file->size == data_len);

    char *read_buffer = kmalloc(data_len + 1);
    assert(read_buffer != NULL);
    ssize_t bytes_read = vfs_read(file, read_buffer, data_len);
    assert(bytes_read == (ssize_t)data_len);
    read_buffer[bytes_read] = '\0';

    assert(strcmp(read_buffer, data_to_write) == 0);
    kfree(read_buffer);

    cleanup(file, ctx, mock_bdev);
}

static void test_sfs_error_handling() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;
    int open_flags; // Declared here

    const char *filename = TEST_FILENAME;
    const char *non_existent_file = "non_existent.txt";
    const char *data = "some data";
    const size_t data_len = strlen(data);

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Test create in a non-existent directory
    err = vfs_create(ctx, "/non_existent_dir/new_file", S_IFREG);
    assert(err == ERR_NOT_FOUND);

    // Test opening a non-existent file without O_CREAT
    err = vfs_open(ctx, non_existent_file, O_RDONLY, &file);
    assert(err == ERR_NOT_FOUND);
    assert(file == NULL);

    // Test opening a non-existent file with O_CREAT | O_EXCL
    int open_flags_create_excl = O_RDWR | O_CREAT | O_EXCL;
    err = vfs_open(ctx, filename, open_flags_create_excl, &file);
    assert(err == OK);
    assert(file != NULL);
    err = vfs_close(file); file = NULL;
    assert(err == OK);
    err = vfs_unlink(ctx, filename);
    assert(err == OK);
    
    // Test opening an existing file with O_CREAT | O_EXCL
    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);
    err = vfs_open(ctx, filename, open_flags_create_excl, &file);
    assert(err == ERR_ALREADY_EXISTS);
    assert(file == NULL);
    err = vfs_unlink(ctx, filename);
    assert(err == OK);

    // Test writing to a read-only file
    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);
    open_flags = O_RDONLY;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    ssize_t bytes_written = vfs_write(file, data, data_len);
    assert(bytes_written == ERR_NOT_PERMITTED);
    err = vfs_close(file); file = NULL;
    assert(err == OK);
    err = vfs_unlink(ctx, filename);
    assert(err == OK);

    // Test reading from a write-only file
    err = vfs_create(ctx, filename, S_IFREG);
    assert(err == OK);
    open_flags = O_WRONLY | O_CREAT;
    err = vfs_open(ctx, filename, open_flags, &file);
    assert(err == OK);
    char read_buffer[128] = {0};
    ssize_t bytes_read = vfs_read(file, read_buffer, sizeof(read_buffer) - 1);
    assert(bytes_read == ERR_NOT_PERMITTED);
    err = vfs_close(file); file = NULL;
    assert(err == OK);
    err = vfs_unlink(ctx, filename);
    assert(err == OK);

    cleanup(file, ctx, mock_bdev);
}

// --- Tests for Directory Operations ---

static void test_sfs_create_dir_basic() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    inode_t target_inode;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    err = vfs_mkdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    err = vfs_lookup(ctx, TEST_DIR_NAME, &target_inode);
    assert(err == OK);
    assert(!inodes.is_empty(&target_inode));
    assert(inodes.is_dir(&target_inode));
    assert(target_inode.sb != NULL && target_inode.sb == ctx->root_inode.sb);

    // Clean up the created directory (must be empty)
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    cleanup(NULL, ctx, mock_bdev);
}

static void test_sfs_create_dir_nested() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create nested directories: /parent_dir/child_dir
    err = vfs_mkdir(ctx, "/parent_dir");
    assert(err == OK);
    err = vfs_mkdir(ctx, "/parent_dir/child_dir");
    assert(err == OK);

    // Verify existence
    inode_t child_inode;
    err = vfs_lookup(ctx, "/parent_dir/child_dir", &child_inode);
    assert(err == OK);
    assert(!inodes.is_empty(&child_inode));
    assert(inodes.is_dir(&child_inode));

    // Clean up
    err = vfs_rmdir(ctx, "/parent_dir/child_dir"); // Must remove child first
    assert(err == OK);
    err = vfs_rmdir(ctx, "/parent_dir");
    assert(err == OK);

    cleanup(NULL, ctx, mock_bdev);
}

static void test_sfs_remove_empty_dir() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a directory
    err = vfs_mkdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    // Verify it exists
    inode_t dir_inode;
    err = vfs_lookup(ctx, TEST_DIR_NAME, &dir_inode);
    assert(err == OK);

    // Remove the directory
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    // Verify it's gone
    err = vfs_lookup(ctx, TEST_DIR_NAME, &dir_inode);
    assert(err == ERR_NOT_FOUND);

    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_remove_non_empty_dir() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a directory
    err = vfs_mkdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    // Create a file inside the directory
    char file_in_dir[256];
    sprintfn(file_in_dir, sizeof(file_in_dir), "%s/%s", TEST_DIR_NAME, TEST_FILENAME);
    err = vfs_create(ctx, file_in_dir, S_IFREG);
    assert(err == OK);

    // Attempt to remove the non-empty directory
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == ERR_DIR_NOT_EMPTY);

    // Clean up by removing the file first, then the directory
    err = vfs_unlink(ctx, file_in_dir);
    assert(err == OK);
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_list_dir_contents() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *dir_file = NULL;
    error_t err;
    char buffer[256];

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a directory with some contents
    err = vfs_mkdir(ctx, TEST_DIR_NAME);
    assert(err == OK);
    
    char file_path[256];
    sprintfn(file_path, sizeof(file_path), "%s/%s", TEST_DIR_NAME, TEST_FILENAME);
    err = vfs_create(ctx, file_path, S_IFREG);
    assert(err == OK);

    char subdir_path[256];
    sprintfn(subdir_path, sizeof(subdir_path), "%s/subdir", TEST_DIR_NAME);
    err = vfs_mkdir(ctx, subdir_path);
    assert(err == OK);

    // Open the directory for reading
    err = vfs_opendir(ctx, TEST_DIR_NAME, &dir_file);
    assert(err == OK);
    assert(dir_file != NULL);

    // Read directory entries
    int entry_count = 0;
    bool found_dot = false;
    bool found_dotdot = false;
    bool found_file = false;
    bool found_subdir = false;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = vfs_readdir(dir_file, (vfs_dirent_t*)buffer);
        
        if (bytes_read <= 0) {
            break;
        }
        
        vfs_dirent_t* current_entry = (vfs_dirent_t*)buffer;
        
        if (strcmp(current_entry->d_name, ".") == 0) {
            found_dot = true;
        } else if (strcmp(current_entry->d_name, "..") == 0) {
            found_dotdot = true;
        } else if (strcmp(current_entry->d_name, TEST_FILENAME) == 0) {
            found_file = true;
        } else if (strcmp(current_entry->d_name, "subdir") == 0) {
            found_subdir = true;
        }
        entry_count++;
    }

    assert(found_dot);
    assert(found_dotdot);
    assert(found_file);
    assert(found_subdir);
    assert(entry_count >= 4);

    // Test rewinddir
    err = vfs_rewinddir(dir_file);
    assert(err == OK);

    // Read again, should start from the beginning
    entry_count = 0;
    found_dot = false; found_dotdot = false; found_file = false; found_subdir = false;
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = vfs_readdir(dir_file, (vfs_dirent_t*)buffer);
        if (bytes_read <= 0) break;
        
        vfs_dirent_t* current_entry = (vfs_dirent_t*)buffer;
        if (strcmp(current_entry->d_name, ".") == 0) found_dot = true;
        else if (strcmp(current_entry->d_name, "..") == 0) found_dotdot = true;
        else if (strcmp(current_entry->d_name, TEST_FILENAME) == 0) found_file = true;
        else if (strcmp(current_entry->d_name, "subdir") == 0) found_subdir = true;
        entry_count++;
    }
    assert(found_dot && found_dotdot && found_file && found_subdir);
    assert(entry_count >= 4);

    // Close the directory
    err = vfs_closedir(dir_file);
    assert(err == OK);

    // Clean up
    err = vfs_rmdir(ctx, subdir_path);
    assert(err == OK);
    err = vfs_unlink(ctx, file_path);
    assert(err == OK);
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == OK);
    
    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_stat_dir() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    error_t err;
    vfs_stat_t stat_info;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a directory
    err = vfs_mkdir(ctx, TEST_DIR_NAME);
    assert(err == OK);

    // Get stats for the directory
    err = vfs_stat(ctx, TEST_DIR_NAME, &stat_info);
    assert(err == OK);

    // Verify basic stat fields for a directory
    assert(stat_info.st_mode != 0);
    assert(S_ISDIR(stat_info.st_mode));
    assert(stat_info.st_uid == 0);
    assert(stat_info.st_gid == 0);

    // Clean up
    err = vfs_rmdir(ctx, TEST_DIR_NAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_truncate_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;
    vfs_stat_t stat_info;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a file and write data to it
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);
    err = vfs_open(ctx, TEST_FILENAME, O_RDWR | O_CREAT | O_TRUNC, &file);
    assert(err == OK);
    const char *initial_data = "This is some initial data for truncation test.";
    ssize_t written = vfs_write(file, initial_data, strlen(initial_data));
    assert(written == (ssize_t)strlen(initial_data));
    assert(file->size == (size_t)strlen(initial_data));
    err = vfs_close(file); file = NULL;
    assert(err == OK);

    // Verify initial size
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert(stat_info.st_size == (size_t)strlen(initial_data));

    // Truncate the file to a smaller size
    size_t truncated_size = 10;
    err = vfs_truncate(ctx, TEST_FILENAME, truncated_size);
    assert(err == OK);

    // Verify size after truncation
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert(stat_info.st_size == truncated_size);

    // Open and read to verify content is truncated
    err = vfs_open(ctx, TEST_FILENAME, O_RDONLY, &file);
    assert(err == OK);
    char read_buffer[256] = {0};
    ssize_t bytes_read = vfs_read(file, read_buffer, sizeof(read_buffer) - 1);
    assert(bytes_read == (ssize_t)truncated_size);
    read_buffer[bytes_read] = '\0';

    char expected_truncated[256];
    strscpy(expected_truncated, initial_data, truncated_size + 1);
    assert(strcmp(read_buffer, expected_truncated) == 0);
    err = vfs_close(file); file = NULL;
    assert(err == OK);

    // Truncate to zero size
    err = vfs_truncate(ctx, TEST_FILENAME, 0);
    assert(err == OK);
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert(stat_info.st_size == 0);

    // Clean up
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}

// --- Directory Error Tests ---

static void test_sfs_opendir_on_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *dir_file = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a regular file
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);

    // Attempt to open the file as a directory
    err = vfs_opendir(ctx, TEST_FILENAME, &dir_file);
    assert(err == ERR_NOT_A_DIRECTORY);
    assert(dir_file == NULL);

    // Clean up
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_opendir_non_existent() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *dir_file = NULL;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Attempt to open a non-existent directory
    err = vfs_opendir(ctx, "/non_existent_dir", &dir_file);
    assert(err == ERR_NOT_FOUND);
    assert(dir_file == NULL);

    cleanup(NULL, ctx, mock_bdev);;
}

// --- Permission Tests ---

static void test_sfs_chmod_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;
    vfs_stat_t stat_info;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // As root, allow non-root users to create files in root dir, in order to test chmod.
    err = vfs_chmod(ctx, "/", 0777);
    assert(err == OK);

    // Now be a non-root user, so that tests make sense
    ctx->uid = 50;
    ctx->gid = 100;

    // Create a file
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);

    // Get initial stats (mode is usually default from creation mask)
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    
    // Change permissions to read-only for owner (0400)
    uint32_t new_mode = S_IRUSR; // Read by owner
    err = vfs_chmod(ctx, TEST_FILENAME, new_mode);
    assert(err == OK);

    // Verify new permissions
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert((stat_info.st_mode & S_IRWXUGO) == new_mode); // Check only permission bits

    // Try to write to a read-only file (should fail)
    err = vfs_open(ctx, TEST_FILENAME, O_WRONLY, &file);
    assert(err == ERR_NOT_PERMITTED);
    assert(file == NULL);

    // Revert permissions to read-write for owner (0600)
    new_mode = S_IRUSR | S_IWUSR;
    err = vfs_chmod(ctx, TEST_FILENAME, new_mode);
    assert(err == OK);

    // Verify permissions reverted
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert((stat_info.st_mode & S_IRWXUGO) == new_mode);

    // Try to write to a now writable file (should succeed)
    err = vfs_open(ctx, TEST_FILENAME, O_WRONLY, &file);
    assert(err == OK);
    assert(file != NULL);
    ssize_t written_bytes = vfs_write(file, "test", 4);
    assert(written_bytes == 4);
    err = vfs_close(file); file = NULL;
    assert(err == OK);

    // Clean up
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_chown_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL; // Default is root (uid=0)
    error_t err;
    vfs_stat_t stat_info;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context(); // ctx->uid = 0 (root)
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a file (initial owner/group will be root:root if ctx->uid/gid is 0)
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);

    // Get initial stats
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert(stat_info.st_uid == 0);
    assert(stat_info.st_gid == 0);

    // Change owner to uid=50, gid=100
    uid_t new_uid = 50;
    gid_t new_gid = 100;
    err = vfs_chown(ctx, TEST_FILENAME, new_uid, new_gid);
    assert(err == OK);

    // Verify new ownership
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);
    assert(stat_info.st_uid == new_uid);
    assert(stat_info.st_gid == new_gid);

    // Try to chown again by a non-root user (should fail)
    vfs_context_t non_root_ctx;
    memcpy(&non_root_ctx, ctx, sizeof(vfs_context_t));
    non_root_ctx.uid = 50; // Same as new_uid
    non_root_ctx.gid = 100;

    err = vfs_chown(&non_root_ctx, TEST_FILENAME, 51, 101); // Try to change to different user
    assert(err == ERR_NOT_PERMITTED); // Only root can change ownership

    // Clean up
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}


static void test_sfs_remove_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    inode_t target_inode;
    error_t err;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a file
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);

    // Verify it exists
    err = vfs_lookup(ctx, TEST_FILENAME, &target_inode);
    assert(err == OK);
    assert(!inodes.is_empty(&target_inode));

    // Remove the file
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);

    // Verify it's gone
    err = vfs_lookup(ctx, TEST_FILENAME, &target_inode);
    assert(err == ERR_NOT_FOUND);

    cleanup(NULL, ctx, mock_bdev);;
}

static void test_sfs_stat_file() {
    mock_block_device_t *mock_bdev = NULL;
    block_device_t *dev = NULL;
    vfs_context_t *ctx = NULL;
    open_file_t *file = NULL;
    error_t err;
    vfs_stat_t stat_info;

    mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    dev = &mock_bdev->base;

    ctx = create_vfs_context();
    assert(ctx != NULL);

    err = simple_fs.mkfs(dev);
    assert(err == OK);

    err = vfs_mount(ctx, "/", dev, simple_fs.ops);
    assert(err == OK);

    // Create a file and write some data
    err = vfs_create(ctx, TEST_FILENAME, S_IFREG);
    assert(err == OK);
    err = vfs_open(ctx, TEST_FILENAME, O_WRONLY, &file);
    assert(err == OK);
    const char* data = "Hello, stat test!";
    ssize_t written_bytes = vfs_write(file, data, strlen(data));
    assert(written_bytes == (ssize_t)strlen(data));
    err = vfs_close(file); file = NULL;
    assert(err == OK);

    // Get stats for the file
    err = vfs_stat(ctx, TEST_FILENAME, &stat_info);
    assert(err == OK);

    // Verify basic stat fields for a file
    assert(stat_info.st_mode != 0);
    assert(S_ISREG(stat_info.st_mode));
    assert(stat_info.st_uid == 0);
    assert(stat_info.st_gid == 0);
    assert(stat_info.st_size == (size_t)strlen(data));
    assert(stat_info.st_blocks > 0); // Should occupy at least one block

    // Clean up
    err = vfs_unlink(ctx, TEST_FILENAME);
    assert(err == OK);
    cleanup(NULL, ctx, mock_bdev);;
}

// -------------------------------------------------------------------------

static sfs_mount_data *create_sfs_mount_data(block_device_t *dev) {
    sfs_mount_data *md = (sfs_mount_data *)kmalloc(sizeof(sfs_mount_data));
    stored_superblock *sb = (stored_superblock *)kmalloc(sizeof(stored_superblock));

    memset(md, 0, sizeof(sfs_mount_data));
    memset(sb, 0, sizeof(stored_superblock));

    sb->direntry_size             = sizeof(stored_dir_entry);
    sb->inode_size                = sizeof(stored_inode);
    sb->num_inodes                = TEST_FS_NUM_INODES;
    sb->sector_size               = dev->block_size;
    sb->sectors_per_block         = TEST_FS_BLOCK_SIZE / dev->block_size;
    sb->block_size_in_bytes       = TEST_FS_BLOCK_SIZE;
    sb->blocks_in_device          = TEST_FS_NUM_BLOCKS;
    sb->inodes_per_block          = TEST_FS_BLOCK_SIZE / sizeof(stored_inode);
    sb->ranges_per_block          = TEST_FS_BLOCK_SIZE / sizeof(block_range);
    sb->inodes_bitmap_first_block = 1; // zero is superblock
    sb->inodes_bitmap_num_blocks  = 1; // in our case
    sb->inodes_array_first_block  = 2; 
    sb->inodes_array_num_blocks   = TEST_INODES_NUM_BLOCKS;
    sb->blocks_bitmap_first_block = sb->inodes_array_first_block + TEST_INODES_NUM_BLOCKS;
    sb->blocks_bitmap_num_blocks  = 1; // in our case

    backed_cache_backend block_backend = {
        .load = sfs_block_cache_backend_load,
        .write = sfs_block_cache_backend_write,
        .context = md
    };

    backed_cache_backend inode_backend = {
        .load = sfs_inode_cache_backend_load,
        .write = sfs_inode_cache_backend_write,
        .context = md
    };

    md->dev = dev;
    md->superblock = sb;

    md->inode_cache = create_backed_cache(sizeof(stored_inode), 64, inode_backend);
    md->inode_bitmap = create_bitmap(TEST_FS_NUM_INODES, TEST_FS_BLOCK_SIZE);
    md->block_cache = create_backed_cache(TEST_FS_BLOCK_SIZE, 16, block_backend);
    md->block_bitmap = create_bitmap(TEST_FS_NUM_BLOCKS, TEST_FS_BLOCK_SIZE);

    md->block_bitmap->ops->mark_used(md->block_bitmap, 0);
    for (size_t i = 0; i < md->superblock->inodes_bitmap_num_blocks; i++) md->block_bitmap->ops->mark_used(md->block_bitmap, md->superblock->inodes_bitmap_first_block + i);
    for (size_t i = 0; i < md->superblock->inodes_array_num_blocks;  i++) md->block_bitmap->ops->mark_used(md->block_bitmap, md->superblock->inodes_array_first_block  + i);
    for (size_t i = 0; i < md->superblock->blocks_bitmap_num_blocks; i++) md->block_bitmap->ops->mark_used(md->block_bitmap, md->superblock->blocks_bitmap_first_block + i);
    
    md->generic_block_buffer = kmalloc(TEST_FS_BLOCK_SIZE);

    return md;
}

static void cleanup_sfs_mount_data(sfs_mount_data *md) {
    if (md->block_bitmap)         md->block_bitmap->ops->destroy(md->block_bitmap);
    if (md->inode_bitmap)         md->inode_bitmap->ops->destroy(md->inode_bitmap);
    if (md->block_cache)          md->block_cache->ops->destroy(md->block_cache);
    if (md->inode_cache)          md->inode_cache->ops->destroy(md->inode_cache);
    if (md->generic_block_buffer) kfree(md->generic_block_buffer);
    if (md->superblock)           kfree(md->superblock);
    kfree(md);
}

static void mock_range_block(sfs_mount_data *md, block_no_t block_no, uint32_t range0_first, uint32_t range0_count, uint32_t range1_first, uint32_t range1_count) {
    error_t err;
    
    err = sfs_cached_fill(md, block_no, 0);
    assert(err == 0);

    block_range ranges[2];
    ranges[0].first_block_no = range0_first;
    ranges[0].blocks_count   = range0_count;
    ranges[1].first_block_no = range1_first;
    ranges[1].blocks_count   = range1_count;

    err = sfs_cached_write(md, block_no, 0, ranges, sizeof(ranges));
    assert(err == 0);
}

static void mock_range_block_full(sfs_mount_data *md, block_no_t block_no, uint32_t first_range_block_no) {
    error_t err;

    block_range *range_arr = (block_range *)kmalloc(TEST_FS_BLOCK_SIZE);
    assert(range_arr != NULL);

    int ranges_per_block = TEST_FS_BLOCK_SIZE / sizeof(block_range);
    block_no_t num = first_range_block_no;

    for (int i = 0; i < ranges_per_block; i++) {
        range_arr[i].first_block_no = num;
        range_arr[i].blocks_count = 1;
        num += 2;
    }

    // log_debug_hex(range_arr, TEST_FS_BLOCK_SIZE, 0);
    err = sfs_cached_write(md, block_no, 0, range_arr, TEST_FS_BLOCK_SIZE);
    assert(err == OK);

    kfree(range_arr);
}

static void log_range_block(sfs_mount_data *md, block_no_t block_no) {
    block_range first_ranges[2];
    block_range last_ranges[2];
    error_t err;
    
    int range_count = TEST_FS_BLOCK_SIZE / sizeof(block_range);

    err = sfs_cached_read(md, block_no, 0, first_ranges, sizeof(first_ranges));
    assert(err == OK);
    err = sfs_cached_read(md, block_no, (range_count - 2) * sizeof(block_range), last_ranges, sizeof(last_ranges));
    assert(err == OK);

    log_debug("block[%d]: range[%d]={%u,%u}, range[%d]={%u,%u}, ..., range[%d]={%u,%u}, range[%d]={%u,%u}",
        block_no,
                      0, first_ranges[0].first_block_no, first_ranges[0].blocks_count,
                      1, first_ranges[1].first_block_no, first_ranges[1].blocks_count,
        range_count - 2, last_ranges[0].first_block_no,  last_ranges[0].blocks_count,
        range_count - 1, last_ranges[1].first_block_no,  last_ranges[1].blocks_count
    );
}

static uint32_t get_range_block_value(sfs_mount_data *md, block_no_t block_no, int range_no, bool is_count) {
    block_range range;
    error_t err = sfs_cached_read(md, block_no, range_no * sizeof(block_range), &range, sizeof(range));
    assert(err == 0);
    return is_count ? range.blocks_count : range.first_block_no;
}

static void sfs_bottom_up_prove_dev_block_io_works(mock_block_device_t *mdev) {
    
    stored_superblock sb = { .sectors_per_block = (TEST_FS_BLOCK_SIZE / TEST_DISK_BLOCK_SIZE) };
    sfs_mount_data md = { .superblock = &sb, .dev = &mdev->base };
    char *block = kmalloc(TEST_FS_BLOCK_SIZE);
    error_t err;

    // clean device
    memset(mdev->memory, 0, mdev->size_bytes);
    assert(memchk(mdev->memory + TEST_FS_BLOCK_SIZE * 0, 0x00, TEST_FS_BLOCK_SIZE * 5));
    
    // write to device, ensure written
    memset(block, 0xFF, TEST_FS_BLOCK_SIZE);
    err = sfs_block_write_to_device(&md, 2, block);
    assert(err == 0);
    assert(memchk(mdev->memory + TEST_FS_BLOCK_SIZE * 0, 0x00, TEST_FS_BLOCK_SIZE * 2));
    assert(memchk(mdev->memory + TEST_FS_BLOCK_SIZE * 2, 0xFF, TEST_FS_BLOCK_SIZE * 1));
    assert(memchk(mdev->memory + TEST_FS_BLOCK_SIZE * 3, 0x00, TEST_FS_BLOCK_SIZE * 2));

    // reading out of bounds fails
    err = sfs_block_read_from_device(&md, 0xFFFFFF, block);
    assert(err < 0);

    // write to device, read into block
    memset(mdev->memory + TEST_FS_BLOCK_SIZE * 2, 0x55, TEST_FS_BLOCK_SIZE * 1);
    err = sfs_block_read_from_device(&md, 2, block);
    assert(err == 0);
    assert(memchk(block, 0x55, TEST_FS_BLOCK_SIZE));

    // writing out of bounds fails
    err = sfs_block_write_to_device(&md, 0xFFFFFF, block);
    assert(err < 0);

    kfree(block);
}

static void sfs_bottom_up_prove_cached_io_works(mock_block_device_t *mdev) {

    sfs_mount_data *md = create_sfs_mount_data(&mdev->base);
    char buffer[8];
    error_t err;

    // read from disk
    memset(mdev->memory, 0, mdev->size_bytes);
    memset(mdev->memory + TEST_FS_BLOCK_SIZE * 1, 'A', 4);
    err = sfs_cached_read(md, 1, 0, buffer, 8);
    assert(err == OK);
    assert(memcmp(buffer, "AAAA\0\0\0\0", 8) == 0);

    // write to cache, at offset
    memcpy(buffer, "BBBB", 4);
    err = sfs_cached_write(md, 1, 4, buffer, 4);
    assert(err == OK);

    // destroy disk data, just to prove cache maintains data
    memset(mdev->memory, 0, mdev->size_bytes);
    assert(memcmp(mdev->memory + TEST_FS_BLOCK_SIZE * 1, "\0\0\0\0\0\0\0\0", 8) == 0);

    // verify cache read, verify disk was not touched
    err = sfs_cached_read(md, 1, 0, buffer, 8);
    assert(err == OK);
    assert(memcmp(buffer, "AAAABBBB", 8) == 0);
    assert(memcmp(mdev->memory + TEST_FS_BLOCK_SIZE * 1, "\0\0\0\0\0\0\0\0", 8) == 0);

    // flush and we should see it on disk
    err = md->block_cache->ops->flush_all(md->block_cache);
    assert(err == OK);
    assert(memcmp(mdev->memory + TEST_FS_BLOCK_SIZE * 1, "AAAABBBB", 8) == 0);

    // cleanup
    cleanup_sfs_mount_data(md);
}

void sfs_bottom_up_prove_block_allocate_release_works(mock_block_device_t *mdev) {

    // we need both bitmap and cache for this.
    sfs_mount_data *md = create_sfs_mount_data(&mdev->base);
    bitmap_t *bmp = md->block_bitmap;
    error_t err;
    uint64_t block_no;

    // if preferred is available, it is returned
    err = sfs_allocate_new_block(md, 24, &block_no);
    assert(err == OK);
    assert(block_no == 24);

    // if preferred not available, something else is returned
    for (unsigned i = 0; i < 32; i++) bmp->ops->mark_used(bmp, i);
    err = sfs_allocate_new_block(md, 24, &block_no);
    assert(err == OK);
    assert(block_no == 32);
    
    // if none available, fail
    bmp->ops->mark_all_used(bmp);
    err = sfs_allocate_new_block(md, 24, &block_no);
    assert(err == ERR_NO_SPACE_LEFT);


    // also test all release functions
    block_range ranges[8];

    // release one block
    bmp->ops->mark_all_used(bmp);
    assert(bmp->ops->is_used(bmp, 24));
    sfs_release_block(md, 24);
    assert(bmp->ops->is_free(bmp, 24));

    // release one range
    bmp->ops->mark_all_used(bmp);
    assert(bmp->ops->is_used(bmp, 23));
    assert(bmp->ops->is_used(bmp, 24));
    assert(bmp->ops->is_used(bmp, 25));
    assert(bmp->ops->is_used(bmp, 26));
    ranges[0].first_block_no = 24;
    ranges[0].blocks_count = 2;
    sfs_release_block_range(md, ranges[0]);
    assert(bmp->ops->is_used(bmp, 23));
    assert(bmp->ops->is_free(bmp, 24));
    assert(bmp->ops->is_free(bmp, 25));
    assert(bmp->ops->is_used(bmp, 26));

    // release array of ranges
    bmp->ops->mark_all_used(bmp);
    memset(ranges, 0, sizeof(ranges));
    ranges[0].first_block_no = 24;
    ranges[0].blocks_count = 2;
    ranges[1].first_block_no = 27;
    ranges[1].blocks_count = 2;
    sfs_release_block_range_array(md, ranges, sizeof(ranges)/sizeof(ranges[0]));
    assert(bmp->ops->is_used(bmp, 23));
    assert(bmp->ops->is_free(bmp, 24));
    assert(bmp->ops->is_free(bmp, 25));
    assert(bmp->ops->is_used(bmp, 26));
    assert(bmp->ops->is_free(bmp, 27));
    assert(bmp->ops->is_free(bmp, 28));
    assert(bmp->ops->is_used(bmp, 29));

    // cleanup
    cleanup_sfs_mount_data(md);
}

void sfs_bottom_up_prove_block_expansion_works(mock_block_device_t *mdev) {
    sfs_mount_data *md = create_sfs_mount_data(&mdev->base);
    block_range ranges[4];
    bool overflow;
    block_no_t new_block_no, indirect_block_no;
    error_t err;
    
    // ranges in memory: with empty range
    memset(ranges, 0, sizeof(ranges));
    err = sfs_expand_range_array(md, ranges, 4, &overflow, &new_block_no);
    assert(err == OK);
    assert(ranges[0].first_block_no > 0);
    assert(ranges[0].blocks_count > 0);
    assert(ranges[1].first_block_no == 0);
    assert(ranges[1].blocks_count == 0);
    assert(overflow == false);
    assert(new_block_no > 0);

    // ranges in memory: with ability to expand last range
    memset(ranges, 0, sizeof(ranges));
    ranges[0].first_block_no = 24;
    ranges[0].blocks_count = 2;
    ranges[1].first_block_no = 48;
    ranges[1].blocks_count = 2;
    md->block_bitmap->ops->mark_free(md->block_bitmap, 50);
    err = sfs_expand_range_array(md, ranges, 4, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    assert(new_block_no > 0);
    assert(ranges[1].blocks_count == 3);

    
    // ranges in memory: with needing new range
    memset(ranges, 0, sizeof(ranges));
    ranges[0].first_block_no = 24;
    ranges[0].blocks_count = 2;
    ranges[1].first_block_no = 48;
    ranges[1].blocks_count = 2;
    ranges[2].first_block_no = 0;
    ranges[2].blocks_count = 0;
    md->block_bitmap->ops->mark_used(md->block_bitmap, 50);
    err = sfs_expand_range_array(md, ranges, 4, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    assert(new_block_no > 0);
    assert(ranges[2].first_block_no > 0);
    assert(ranges[2].blocks_count == 1);
    
    // ranges in memory: with overflow
    memset(ranges, 0, sizeof(ranges));
    ranges[0].first_block_no = 10; ranges[0].blocks_count = 2;
    ranges[1].first_block_no = 20; ranges[1].blocks_count = 2;
    ranges[2].first_block_no = 30; ranges[2].blocks_count = 2;
    ranges[3].first_block_no = 40; ranges[3].blocks_count = 2;
    md->block_bitmap->ops->mark_used(md->block_bitmap, 42);
    err = sfs_expand_range_array(md, ranges, 4, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == true);
    

    // indirect block: simple test, as it's a wrapper to above
    md->block_bitmap->ops->mark_all_free(md->block_bitmap);
    md->block_bitmap->ops->mark_used(md->block_bitmap, 0);
    md->block_bitmap->ops->mark_used(md->block_bitmap, 52);
    mock_range_block(md, 24, 50, 2, 0, 0);
    err = sfs_expand_range_block(md, 24, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    assert(new_block_no > 0);
    assert(get_range_block_value(md, 24, 0, false) == 50);
    assert(get_range_block_value(md, 24, 0, true) == 2);
    assert(get_range_block_value(md, 24, 1, false) > 0);
    assert(get_range_block_value(md, 24, 1, true) == 1);


    // double indirect blocks: no indirects yet
    mock_range_block(md, 120, 0, 0, 0, 0);
    log_range_block(md, 120);
    err = sfs_expand_dbl_indirect_block(md, 120, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    assert(new_block_no > 0);
    log_range_block(md, 120);
    sfs_cached_read(md, 120, 0, &indirect_block_no, sizeof(indirect_block_no));
    log_range_block(md, indirect_block_no);
    assert(get_range_block_value(md, 120, 0, false) > 0);
    assert(get_range_block_value(md, 120, 0, true) == 1);
    assert(get_range_block_value(md, indirect_block_no, 0, false) > 0);
    assert(get_range_block_value(md, indirect_block_no, 0, true) == 1);


    // expanding existing indirect
    mock_range_block(md, 220, 240, 1, 0, 0);
    mock_range_block(md, 240, 260, 1, 0, 0);
    log_range_block(md, 220);
    log_range_block(md, 240);
    log_range_block(md, 260);
    md->block_bitmap->ops->mark_free(md->block_bitmap, 261);
    err = sfs_expand_dbl_indirect_block(md, 220, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    log_range_block(md, 220);
    log_range_block(md, 240);
    log_range_block(md, 260);
    assert(new_block_no == 261);


    // adding new indirect block (we need an indirect that is full)
    mock_range_block(md, 320, 340, 1, 0, 0);
    mock_range_block_full(md, 340, 500);
    log_range_block(md, 320);
    log_range_block(md, 340);
    md->block_bitmap->ops->mark_used(md->block_bitmap, 500 + (TEST_FS_BLOCK_SIZE/sizeof(block_range))*2 - 1); // disable would-be range block
    md->block_bitmap->ops->mark_used(md->block_bitmap, 341); // disable would-be indirect block
    err = sfs_expand_dbl_indirect_block(md, 320, &overflow, &new_block_no);
    assert(err == OK);
    assert(overflow == false);
    log_range_block(md, 320);
    log_range_block(md, 340);
    assert(new_block_no > 0);
    assert(get_range_block_value(md, 320, 0, false) == 340);
    assert(get_range_block_value(md, 320, 0, true) == 1);
    assert(get_range_block_value(md, 320, 1, false) > 0);
    assert(get_range_block_value(md, 320, 1, true) == 1);
    sfs_cached_read(md, 320, sizeof(block_range) * 1, &ranges, sizeof(block_range));
    indirect_block_no = ranges[0].first_block_no;
    log_range_block(md, indirect_block_no);
    assert(get_range_block_value(md, indirect_block_no, 0, false) > 0);
    assert(get_range_block_value(md, indirect_block_no, 0, true) == 1);

    
    // with all slots exhausted, even the double indirect one
    mock_range_block_full(md, 120, 500);
    sfs_cached_read(md, 120, TEST_FS_BLOCK_SIZE - sizeof(block_range), ranges, sizeof(block_range)); // grab last range
    indirect_block_no = ranges[0].first_block_no + ranges[0].blocks_count - 1; // grab last indirect block no
    mock_range_block_full(md, indirect_block_no, 1500); // make last indirect full too.
    sfs_cached_read(md, indirect_block_no, TEST_FS_BLOCK_SIZE - sizeof(block_range), ranges, sizeof(block_range)); // grab last range
    new_block_no = ranges[0].first_block_no + ranges[0].blocks_count; // what would expand into
    md->block_bitmap->ops->mark_used(md->block_bitmap, new_block_no); // mark unavailable to cause failure
    err = sfs_expand_dbl_indirect_block(md, 120, &overflow, &new_block_no);
    assert(err == ERR_NO_SPACE_LEFT);
    assert(overflow == true);
}

void sfs_bottom_up_prove_inode_cache_works(mock_block_device_t *mdev) {
    sfs_mount_data *md = create_sfs_mount_data(&mdev->base);
    stored_inode inode;
    error_t err;
    stored_inode *inodes_array = (stored_inode *)(mdev->memory + (TEST_FS_BLOCK_SIZE * md->superblock->inodes_array_first_block));

    // read from disk
    memset(mdev->memory, 0xFF, mdev->size_bytes);
    inodes_array[2] = (stored_inode){ .allocated_blocks = 123 };

    memset(&inode, 0, sizeof(stored_inode));
    err = sfs_load_inode2(md, 2, &inode);
    assert(err == OK);
    assert(inode.allocated_blocks == 123);

    // write to cache, at offset
    inode.allocated_blocks = 456;
    err = sfs_save_inode2(md, 2, &inode);
    assert(err == OK);

    // destroy disk data, just to prove cache maintains data
    inodes_array[2] = (stored_inode){ .allocated_blocks = 0xFFFF };
    
    // verify cache read, verify disk was not touched
    memset(&inode, 0, sizeof(stored_inode));
    err = sfs_load_inode2(md, 2, &inode);
    assert(err == OK);
    assert(inode.allocated_blocks == 456);

    // flush and we should see it on disk
    err = md->inode_cache->ops->flush_all(md->inode_cache);
    assert(err == OK);
    assert(inodes_array[2].allocated_blocks == 456);

    // cleanup
    cleanup_sfs_mount_data(md);
}

void sfs_bottom_up_prove_single_inode_data_operations_work(mock_block_device_t *mdev) {
    // all inode based operations: resolving, writing (both between blocks and expanding), reading, truncating
    stored_inode _stored_inode;
    stored_inode *n = &_stored_inode;
    block_no_t block_no;
    error_t err;
    ssize_t bytes;
    
    // prepare
    sfs_mount_data *md = create_sfs_mount_data(&mdev->base);
    char *block = kmalloc(TEST_FS_BLOCK_SIZE);
    assert(block != NULL);
    for (int i = 0; i < TEST_FS_BLOCK_SIZE; i++)
        block[i] = '0' + (i & 0x3F); // 64 combinations
    memset(n, 0, sizeof(stored_inode));

    // attempt to read empty file
    bytes = sfs_read_file_data(md, n, 0, block, 16);
    assert(bytes == 0); // no bytes read

    // first allocation & writing
    // (seem node_cache is needed and expanding is marking it dirty, TODO: i think this has to change)
    bytes = sfs_write_file_data(md, n, 0, block, 8);
    assert(bytes == 8);
    assert(n->allocated_blocks == 1);
    assert(n->file_size == 8);
    assert(n->ranges[0].first_block_no > 0);
    assert(n->ranges[0].blocks_count == 1);

    // attempt to read 16 bytes, should read 8
    bytes = sfs_read_file_data(md, n, 0, block, 16);
    assert(bytes == 8); // no bytes read
    assert(memcmp(block, "01234567", 8) == 0);
    
    // write a whole block at offset 8, should cover 2 blocks
    bytes = sfs_write_file_data(md, n, 8, block, TEST_FS_BLOCK_SIZE);
    assert(bytes == TEST_FS_BLOCK_SIZE);
    assert(n->file_size == 8 + TEST_FS_BLOCK_SIZE);
    assert(n->allocated_blocks == 2);
    assert(n->ranges[0].first_block_no > 0);
    assert(n->ranges[0].blocks_count == 2); // assume serial allocation

    // very simple logic here, could push this to indirect check and double indirect check
    err = sfs_node_resolve_data_block(md, n, 0, &block_no);
    assert(err == 0);
    assert(block_no == n->ranges[0].first_block_no);
    err = sfs_node_resolve_data_block(md, n, 1, &block_no);
    assert(err == 0);
    assert(block_no == n->ranges[0].first_block_no + 1);
    err = sfs_node_resolve_data_block(md, n, 999, &block_no);
    assert(err == ERR_NOT_FOUND);

    // attempt to read 16 bytes, should read 16
    bytes = sfs_read_file_data(md, n, 0, block, 16);
    assert(bytes == 16); // no bytes read
    assert(memcmp(block, "0123456701234567", 16) == 0);

    // truncate (TODO: pass in the node, don't read in there)
    err = sfs_node_release_all_data_blocks(md, n);
    assert(err == OK);
    assert(n->allocated_blocks == 0);
    assert(memchk(&n->ranges, 0, sizeof(block_range) * RANGES_IN_INODE) == 0);
    assert(n->indirect_ranges_block_no == 0);
    assert(n->double_indirect_block_no == 0);

    cleanup_sfs_mount_data(md);
    kfree(block);
}

static void test_sfs_internals() {

    // this is white box testing, from inside.
    mock_block_device_t *mock_bdev = create_mock_device(TEST_DISK_SIZE_BYTES, TEST_DISK_BLOCK_SIZE);
    assert(mock_bdev != NULL);
    block_device_t *dev = &mock_bdev->base;

    sfs_bottom_up_prove_dev_block_io_works(mock_bdev);           // block, cluster etc
    sfs_bottom_up_prove_cached_io_works(mock_bdev);              // read/write, flush etc
    sfs_bottom_up_prove_block_allocate_release_works(mock_bdev); // count, find free, mark free / used etc
    sfs_bottom_up_prove_block_expansion_works(mock_bdev);        // extend, direct, indirect, double indirect
    sfs_bottom_up_prove_inode_cache_works(mock_bdev);            // read, write, create new inode, flush etc
    sfs_bottom_up_prove_single_inode_data_operations_work(mock_bdev); // read, write, expand, truncate etc

    // only now we can do file-level operations (create, delete, mkdir, rmdir, truncate etc)

    destroy_mock_device(mock_bdev);
}


// --- Test Suite Entry Point ---
void sfs_unit_tests() {

    test_sfs_internals();

    // the rest is testing sfs as black box, exhausting its functionality
    test_sfs_mkfs_success();
    test_sfs_mount_success();

    test_sfs_create_file_basic();
    test_sfs_create_write_read_close_basic();
    test_sfs_seek_and_offsets();

    test_sfs_write_multiblock_read_verify();
    test_sfs_error_handling();

    // --- Directory Operations Tests ---
    test_sfs_create_dir_basic();
    test_sfs_create_dir_nested();
    test_sfs_remove_empty_dir();
    test_sfs_remove_non_empty_dir();
    test_sfs_remove_file();
    test_sfs_stat_file();
    test_sfs_truncate_file();
    test_sfs_list_dir_contents();
    
    // --- Directory Error Tests ---
    test_sfs_opendir_on_file();
    test_sfs_opendir_non_existent();
    
    // --- Permission Tests ---
    test_sfs_chown_file();
    test_sfs_chmod_file();

    // log_info("* * * * * * * * * *       sfs_unit_tests() ALL PASSED!, freezing       * * * * * * * * * * *"); for(;;);
}


#endif // ENABLE_UNIT_TESTS