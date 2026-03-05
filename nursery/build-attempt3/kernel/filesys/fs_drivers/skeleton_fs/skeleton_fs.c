#include "../fs_driver.h"
#include "../../../include/uapi/errors.h"
#include "../../../memory/kheap.h"
#include "../../../klib/string.h"
#include "../../../klib/cache.h"
#include "../../../klib/list.h"


static error_t _skeleton_fs_probe(block_device_t *dev) {
    // just check if device seems to contain a supported filesystem
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_mount(superblock_t *sb) {
    // create private data, store on sb->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_unmount(superblock_t *sb) {
    // destroy private data from sb->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_sync(superblock_t *sb) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_mkfs(block_device_t *dev) {
    // just check if device seems to contain a supported filesystem
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_get_root_dir(superblock_t *sb, inode_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_lookup(inode_t *dir, const char *name, inode_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_open(inode_t *n, int flags, open_file_t **file_handle) {
    // create private data, store in file->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_close(open_file_t *file) {
    // destroy private data from file->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static ssize_t _skeleton_fs_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    // grab private data and offset from file
    return ERR_NOT_IMPLEMENTED;
}

static ssize_t _skeleton_fs_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    // grab private data and offset from file
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_flush(open_file_t *file) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_opendir(inode_t *dir, open_file_t **dir_handle) {
    // create private data, store in dir_handle->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static ssize_t _skeleton_fs_readdir(open_file_t *dir_handle, vfs_dirent_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_rewinddir(open_file_t *dir_handle) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_closedir(open_file_t *dir_handle) {
    // destroy private data from dir_handle->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_mkdir(inode_t *parent, const char *name, inode_t *out) { 
    // create directory, but also "." and ".."
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_rmdir(inode_t *parent, const char *name) {
    // check if dir is empty or not.
    // remove "." and ".."
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_create(inode_t *parent, const char *name, int type, inode_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_unlink(inode_t *parent, const char *name) {
    // remove directory entry.
    // if inode counter reaches zero, remove file and blocks as well.
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_stat(inode_t *n, vfs_stat_t *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _skeleton_fs_truncate(inode_t *n, size_t size) {
    return ERR_NOT_IMPLEMENTED;
}


static fs_driver_ops_t skeleton_fs_ops = {
    .probe        = _skeleton_fs_probe,
    .mount        = _skeleton_fs_mount,
    .unmount      = _skeleton_fs_unmount,
    .sync         = _skeleton_fs_sync,
    .mkfs         = _skeleton_fs_mkfs,
    .get_root_dir = _skeleton_fs_get_root_dir,
    .lookup       = _skeleton_fs_lookup,
    .open         = _skeleton_fs_open,
    .close        = _skeleton_fs_close,
    .read         = _skeleton_fs_read,
    .write        = _skeleton_fs_write,
    .flush        = _skeleton_fs_flush,
    .opendir      = _skeleton_fs_opendir,
    .readdir      = _skeleton_fs_readdir,
    .rewinddir    = _skeleton_fs_rewinddir,
    .closedir     = _skeleton_fs_closedir,
    .create       = _skeleton_fs_create,
    .unlink       = _skeleton_fs_unlink,
    .mkdir        = _skeleton_fs_mkdir,
    .rmdir        = _skeleton_fs_rmdir,
    .stat         = _skeleton_fs_stat,
    .truncate     = _skeleton_fs_truncate,
};

fs_driver_t skeleton_fs = {
    .name = "Skeleton FS",
    .ops = &skeleton_fs_ops,
    .probe = _skeleton_fs_probe,
    .mkfs = _skeleton_fs_mkfs,
};
