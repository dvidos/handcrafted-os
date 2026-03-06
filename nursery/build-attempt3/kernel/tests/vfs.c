#include "../include/ctypes.h"
#include "../filesys/vfs_api.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "../include/uapi/errors.h"
#include "../logger/logger.h"
#include "framework.h"

MODULE("VFS_UNIT_TEST", LOG_LEVEL_WARN)



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

void test_vfs() {
    int err;

    superblock_t *superblock = kmalloc(sizeof(superblock_t));
    memset(superblock, 0, sizeof(superblock_t));
    superblock->ops->lookup = _mock_filesys_lookup;
    
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
    _mock_filesys_lookup_data[1].result = sh;
    _mock_filesys_lookup_data[1].ret_val = OK;

    logger_set_module_log_level("VFS", LOG_LEVEL_TRACE);

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
}

