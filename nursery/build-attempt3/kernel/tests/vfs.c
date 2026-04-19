#include "../include/ctypes.h"
#include "../filesys/vfs_api.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "../include/uapi/errors.h"
#include "../logger/logger.h"
#include "../include/uapi/vfs_file_flags.h" // Include for O_* flags
#include "framework.h"

MODULE("VFS_UNIT_TEST", LOG_LEVEL_DEBUG) // Changed to DEBUG for more verbose output

// --- Mock File System Driver State and Operations for vfs_open flags tests ---
#define MOCK_FILE_CONTENT_SIZE 1024
typedef struct {
    bool file_exists;
    size_t current_size;
    char content[MOCK_FILE_CONTENT_SIZE];
    bool create_called;
    bool truncate_called;
    size_t write_offset_received;
    size_t read_offset_received;
} mock_fs_state_t;

static mock_fs_state_t _mock_fs_state;

static error_t _mock_fs_create(inode_t *parent, const char *name, int type, inode_t *out) {
    _mock_fs_state.create_called = true;
    if (_mock_fs_state.file_exists) return ERR_ALREADY_EXISTS;
    _mock_fs_state.file_exists = true;
    _mock_fs_state.current_size = 0;
    memset(_mock_fs_state.content, 0, MOCK_FILE_CONTENT_SIZE);
    *out = inodes.create(parent->sb, 123, parent, name, 0); // Mock inode for new file
    return OK;
}

static error_t _mock_fs_truncate(inode_t *n, size_t size) {
    _mock_fs_state.truncate_called = true;
    _mock_fs_state.current_size = size;
    memset(_mock_fs_state.content, 0, MOCK_FILE_CONTENT_SIZE); // Clear content on truncate
    n->size = size; // Update inode size
    return OK;
}

static error_t _mock_fs_open(inode_t *n, int flags, open_file_t **file_handle) {
    *file_handle = open_files.create(n->sb, n);
    (*file_handle)->flags = flags; // Store flags for later checks in VFS write/read
    (*file_handle)->size = _mock_fs_state.current_size; // Initial size from mock state
    return OK;
}

static error_t _mock_fs_close(open_file_t *file) {
    open_files.release(file); // Simply release the open_file_t
    return OK;
}

static ssize_t _mock_fs_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    _mock_fs_state.read_offset_received = offset;
    size_t bytes_to_read = min(len, _mock_fs_state.current_size - offset);
    if (bytes_to_read > 0) {
        memcpy(buf, _mock_fs_state.content + offset, bytes_to_read);
    }
    return (ssize_t)bytes_to_read;
}

static ssize_t _mock_fs_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    _mock_fs_state.write_offset_received = offset;
    size_t bytes_to_write = min(len, MOCK_FILE_CONTENT_SIZE - offset);
    if (bytes_to_write == 0) return ERR_NO_SPACE_LEFT; // Mock overflow
    
    memcpy(_mock_fs_state.content + offset, buf, bytes_to_write);
    _mock_fs_state.current_size = max(_mock_fs_state.current_size, offset + bytes_to_write);
    file->inode.size = _mock_fs_state.current_size; // Update inode size in mock
    return (ssize_t)bytes_to_write;
}

static fs_driver_ops_t mock_driver_ops = {
    .create = _mock_fs_create,
    .truncate = _mock_fs_truncate,
    .open = _mock_fs_open,
    .close = _mock_fs_close,
    .read = _mock_fs_read,
    .write = _mock_fs_write,
    // Other operations not directly tested by vfs_open flags can be NULL or mock_fs_err_not_implemented
};

// Original _mock_filesys_lookup for vfs_resolve tests
struct _mock_filesys_lookup {
    inode_t *dir;
    char *name;
    inode_t *result;
    int ret_val;
} _mock_filesys_lookup_data[4];

int _mock_filesys_lookup(inode_t *dir, char *name, inode_t *result) {
    for (unsigned int i = 0; i < sizeof(_mock_filesys_lookup_data) / sizeof(_mock_filesys_lookup_data[0]); i++) {
        if (dir->location == _mock_filesys_lookup_data[i].dir->location &&
            strcmp(name, _mock_filesys_lookup_data[i].name) == 0)
        {
           *result = clone_inode(_mock_filesys_lookup_data[i].result);
           return _mock_filesys_lookup_data[i].ret_val;
        }
    }

    return 123456;
}

// --- Test function for vfs_open flags ---
void test_vfs_open_flags() {
    unit_test_start("vfs_open flags");

    superblock_t *mock_sb = kmalloc(sizeof(superblock_t));
    memset(mock_sb, 0, sizeof(superblock_t));
    mock_sb->driver = &mock_driver_ops; // Assign our mock driver ops for operations other than lookup

    inode_t mock_parent_inode = inodes.create(mock_sb, 1, NULL, NULL, 0); // Mock parent dir inode
    inode_t mock_file_inode = inodes.create(mock_sb, 2, NULL, NULL, 0); // Mock file inode

    open_file_t *test_file = NULL;
    int err;

    // --- Test 1: O_CREAT - Create a new file ---
    _mock_fs_state = (mock_fs_state_t){0}; // Reset mock state
    _mock_fs_state.file_exists = false; // File does not exist initially
    // To make vfs_lookup return ERR_NOT_FOUND, clear _mock_filesys_lookup_data
    memset(_mock_filesys_lookup_data, 0, sizeof(_mock_filesys_lookup_data));
    
    err = vfs_open("/newfile.txt", O_CREAT | O_WRONLY, &test_file);
    assert_ok(err);
    assert_true(_mock_fs_state.create_called);
    assert_true(_mock_fs_state.file_exists);
    assert_not_null(test_file);
    assert_equals(test_file->flags & (O_CREAT | O_WRONLY), (O_CREAT | O_WRONLY));
    vfs_close(test_file);

    // --- Test 2: O_CREAT | O_EXCL - File already exists, should fail ---
    _mock_fs_state = (mock_fs_state_t){0}; // Reset mock state
    _mock_fs_state.file_exists = true; // File exists in mock state
    
    // Set up _mock_filesys_lookup_data to simulate "/existing.txt" found
    memset(_mock_filesys_lookup_data, 0, sizeof(_mock_filesys_lookup_data));
    _mock_filesys_lookup_data[0].dir = &mock_parent_inode; 
    _mock_filesys_lookup_data[0].name = "existing.txt";
    _mock_filesys_lookup_data[0].result = &mock_file_inode; // Return a mock inode for the existing file
    _mock_filesys_lookup_data[0].ret_val = OK;
    
    err = vfs_open("/existing.txt", O_CREAT | O_EXCL | O_WRONLY, &test_file);
    assert_equals(err, ERR_FILE_EXISTS);
    assert_null(test_file);


    // --- Test 3: O_TRUNC - Truncate an existing file ---
    _mock_fs_state = (mock_fs_state_t){0}; // Reset mock state
    _mock_fs_state.file_exists = true;
    _mock_fs_state.current_size = 100; // File has some content
    memcpy(_mock_fs_state.content, "old content", 11);
    mock_file_inode.size = 100; // Mock inode size
    
    // Set up _mock_filesys_lookup_data to simulate "/truncate.txt" found
    memset(_mock_filesys_lookup_data, 0, sizeof(_mock_filesys_lookup_data));
    _mock_filesys_lookup_data[0].dir = &mock_parent_inode;
    _mock_filesys_lookup_data[0].name = "truncate.txt";
    _mock_filesys_lookup_data[0].result = &mock_file_inode;
    _mock_filesys_lookup_data[0].ret_val = OK;

    err = vfs_open("/truncate.txt", O_TRUNC | O_WRONLY, &test_file);
    assert_ok(err);
    assert_true(_mock_fs_state.truncate_called);
    assert_equals(_mock_fs_state.current_size, 0);
    assert_not_null(test_file);
    vfs_close(test_file);

    // --- Test 4: O_APPEND - Write to end of file ---
    _mock_fs_state = (mock_fs_state_t){0}; // Reset mock state
    _mock_fs_state.file_exists = true;
    _mock_fs_state.current_size = 5;
    memcpy(_mock_fs_state.content, "hello", 5);
    mock_file_inode.size = 5;

    // Set up _mock_filesys_lookup_data to simulate "/append.txt" found
    memset(_mock_filesys_lookup_data, 0, sizeof(_mock_filesys_lookup_data));
    _mock_filesys_lookup_data[0].dir = &mock_parent_inode;
    _mock_filesys_lookup_data[0].name = "append.txt";
    _mock_filesys_lookup_data[0].result = &mock_file_inode;
    _mock_filesys_lookup_data[0].ret_val = OK;

    err = vfs_open("/append.txt", O_APPEND | O_WRONLY, &test_file);
    assert_ok(err);
    assert_not_null(test_file);
    assert_equals(test_file->offset, _mock_fs_state.current_size); // Offset should be at end of initial content

    // Write some data
    const char *append_data = " world";
    ssize_t bytes_written = vfs_write(test_file, (void *)append_data, strlen(append_data));
    assert_true(bytes_written > 0);
    assert_equals(_mock_fs_state.write_offset_received, 5); // Should write at offset 5
    assert_equals(_mock_fs_state.current_size, 5 + strlen(append_data));
    assert_string_equals(_mock_fs_state.content, "hello world");
    vfs_close(test_file);

    unit_test_end();
}

void test_vfs() {
    int err;

    superblock_t *superblock = kmalloc(sizeof(superblock_t));
    memset(superblock, 0, sizeof(superblock_t));
    superblock->ops->lookup = _mock_filesys_lookup; // Use original mock lookup for vfs_resolve tests
    
    inode_t *root = create_inode(superblock, "/", 0, NULL);
    inode_t *curr = create_inode(superblock, "home", 2, root);
    inode_t *target;

    root->flags = FD_DIR;
    curr->flags = FD_DIR;

    // make sure that root dir can be returned, even if we have no current dir.
    err = vfs_resolve("/", root, NULL, false, &target);
    assert(err == OK);
    assert(target != NULL);
    assert(target->superblock = superblock);
    assert(target->location == root->location);
    assert(target != root); // we are supposed to return a clone, not the same reference
    destroy_inode(target);
    target = NULL;

    // test root returned for the parent of something
    err = vfs_resolve("/something", root, NULL, true, &target);
    assert(err == OK);
    assert(target != NULL);
    assert(target->superblock = superblock);
    assert(target->location == root->location);
    assert(target != root); // we are supposed to return a clone, not the same reference
    destroy_inode(target);
    target = NULL;

    // also, curr dir as well.
    err = vfs_resolve(".", root, curr, false, &target);
    assert(err == OK);
    assert(target != NULL);
    assert(target->superblock = superblock);
    assert(target->location == curr->location);
    assert(target != curr); // we are supposed to return a clone, not the same reference
    destroy_inode(target);
    target = NULL;

    // test curr dir returned for the parent of a file
    err = vfs_resolve("file", root, curr, true, &target);
    assert(err == OK);
    assert(target != NULL);
    assert(target->superblock = superblock);
    assert(target->location == curr->location);
    assert(target != curr); // we are supposed to return a clone, not the same reference
    destroy_inode(target);
    target = NULL;



    // make sure absolute paths can be returned, even if we have no current dir
    // given root, and bin, return bin
    inode_t *bin = create_inode(superblock, "bin", 4, root);
    bin->flags |= FD_DIR;
    inode_t *sh = create_inode(superblock, "sh", 6, bin);
    memset(_mock_filesys_lookup_data, 0, sizeof(_mock_filesys_lookup_data));

    _mock_filesys_lookup_data[0].dir = root;
    _mock_filesys_lookup_data[0].name = "bin";
    _mock_filesys_lookup_data[0].result = bin;
    _mock_filesys_lookup_data[0].ret_val = OK;

    _mock_filesys_lookup_data[1].dir = bin;
    _mock_filesys_lookup_data[1].name = "sh";
    _mock_filesys_lookup_data[1].ret_val = OK;
    _mock_filesys_lookup_data[1].result = sh;

    logger_set_module_log_level("VFS", LOG_LEVEL_DEBUG);

    // see resolution of bin, even if curr dir does not exist.
    err = vfs_resolve("/bin", root, NULL, false, &target);
    log_info("Returned %d", err);
    assert(err == OK);
    assert(target != NULL);
    assert(target->location == bin->location);
    destroy_inode(target);
    target = NULL;

    // see resolution of non existant
    err = vfs_resolve("/something_entirely_missing", root, NULL, false, &target);
    assert(err == ERR_NOT_FOUND);
    assert(target == NULL);
    
    // see resolution of nested path
    err = vfs_resolve("/bin/init", root, NULL, false, &target);
    assert(err == OK);
    assert(target != NULL);
    assert(target->superblock = superblock);
    assert(target->location == sh->location); // same location...
    assert(target != sh); // but not same pointer
    destroy_inode(target);
    target = NULL;

    // Call the new test function
    test_vfs_open_flags();
}
