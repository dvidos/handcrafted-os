#include "../fs_driver_ops.h"
#include "../../../include/uapi/errors.h"
#include "../../../memory/kheap.h"
#include "../../../klib/string.h"
#include "../../../klib/cache.h"
#include "../../../klib/list.h"


// -----------------------------------------------------------
// dummy fs:
//   /file.txt  "hello from file.txt"
// -----------------------------------------------------------
// root dir: inode 0
// file: inode 1
// -----------------------------------------------------------
#define ROOT_DIR_INODE    0
#define TEXT_FILE_INODE   1
#define TEXT_FILE_NAME    "file.txt"
#define TEXT_FILE_CONTENTS  "hello from file.txt"


static error_t _dummy_fs_probe(block_device_t *dev) {
    // just check if device seems to contain a supported filesystem
    return OK;
}

static error_t _dummy_fs_mount(superblock_t *sb) {
    // create private data, store on sb->driver_priv_data
    return OK;
}

static error_t _dummy_fs_unmount(superblock_t *sb) {
    // destroy private data from sb->driver_priv_data
    return OK;
}

static error_t _dummy_fs_sync(superblock_t *sb) {
    return OK;
}

static error_t _dummy_fs_mkfs(block_device_t *dev) {
    return OK;
}

static error_t _dummy_fs_get_root_dir(superblock_t *sb, file_descriptor_t **out) {
    *out = file_descriptors.create(sb, ROOT_DIR_INODE, NULL, "/");
    return OK;
}

static error_t _dummy_fs_lookup(file_descriptor_t *dir, const char *name, file_descriptor_t **out) {
    if (dir->inode != ROOT_DIR_INODE)
        return ERR_NOT_FOUND;

    if (strcmp(name, TEXT_FILE_NAME) != 0)
        return ERR_NOT_FOUND;
    
    *out = file_descriptors.create(dir->sb, TEXT_FILE_INODE, dir, TEXT_FILE_NAME);
    return OK;
}

static error_t _dummy_fs_open(file_descriptor_t *fd, int flags, open_file_t **file_handle) {
    // create private data, store in file->driver_priv_data
    if (fd->inode != TEXT_FILE_INODE)
        return ERR_BAD_ARGUMENT;

    // let's assume we create somerthing 
    *file_handle = open_files.create(fd->sb, fd);
    return OK;
}

static error_t _dummy_fs_close(open_file_t *file) {
    // destroy private data from file->driver_priv_data
    return OK;
}

static ssize_t _dummy_fs_read(open_file_t *file, void *buf, size_t len, off_t offset) {
    // grab private data and offset from file
    if (file->fd->inode != TEXT_FILE_INODE)
        return ERR_BAD_ARGUMENT;

    int text_offset = file->offset;
    if (text_offset >= strlen(TEXT_FILE_CONTENTS))
        return 0;
    
    int file_size = strlen(TEXT_FILE_CONTENTS + text_offset);
    int read_size = file_size < len ? file_size : len;
    strncpy(buf, TEXT_FILE_CONTENTS, read_size);

    return read_size;
}

static ssize_t _dummy_fs_write(open_file_t *file, const void *buf, size_t len, off_t offset) {
    // grab private data and offset from file
    return ERR_NOT_PERMITTED;
}

static error_t _dummy_fs_flush(open_file_t *file) {
    return OK;
}

static error_t _dummy_fs_opendir(file_descriptor_t *dir, open_file_t **dir_handle) {
    // create private data, store in dir_handle->driver_priv_data
    if (dir->inode != ROOT_DIR_INODE)
        return ERR_BAD_ARGUMENT;
    
    *dir_handle = open_files.create(dir->sb, dir);
    return OK;
}

static ssize_t _dummy_fs_readdir(open_file_t *dir_handle, struct dirent *out) {
    if (dir_handle->offset == 0) {
        *out = file_descriptors.create(dir_handle->sb, TEXT_FILE_INODE, dir_handle->fd, TEXT_FILE_NAME);
        return OK;
    } else {
        return ERR_EOF;
    }
}

static error_t _dummy_fs_rewinddir(open_file_t *dir_handle) {
    dir_handle->offset = 0;
    return OK;
}

static error_t _dummy_fs_closedir(open_file_t *dir_handle) {
    // destroy private data from dir_handle->driver_priv_data
    return OK;
}

static error_t _dummy_fs_mkdir(file_descriptor_t *parent, const char *name) { 
    // create directory, but also "." and ".."
    return ERR_NOT_PERMITTED;
}

static error_t _dummy_fs_rmdir(file_descriptor_t *parent, const char *name) {
    // check if dir is empty or not.
    // remove "." and ".."
    return ERR_NOT_PERMITTED;
}

static error_t _dummy_fs_create(file_descriptor_t *parent, const char *name, int type, file_descriptor_t **out) {
    return ERR_NOT_PERMITTED;
}

static error_t _dummy_fs_unlink(file_descriptor_t *parent, const char *name) {
    // remove directory entry.
    // if inode counter reaches zero, remove file and blocks as well.
    return ERR_NOT_PERMITTED;
}

static error_t _dummy_fs_stat(file_descriptor_t *fd, struct stat *out) {
    return ERR_NOT_IMPLEMENTED;
}

static error_t _dummy_fs_truncate(file_descriptor_t *fd, size_t size) {
    return ERR_NOT_IMPLEMENTED;
}



fs_driver_ops_t dummy_fs_ops = {
    .probe        = _dummy_fs_probe,
    .mount        = _dummy_fs_mount,
    .unmount      = _dummy_fs_unmount,
    .sync         = _dummy_fs_sync,
    .mkfs         = _dummy_fs_mkfs,
    .get_root_dir = _dummy_fs_get_root_dir,
    .lookup       = _dummy_fs_lookup,
    .open         = _dummy_fs_open,
    .close        = _dummy_fs_close,
    .read         = _dummy_fs_read,
    .write        = _dummy_fs_write,
    .flush        = _dummy_fs_flush,
    .opendir      = _dummy_fs_opendir,
    .readdir      = _dummy_fs_readdir,
    .rewinddir    = _dummy_fs_rewinddir,
    .closedir     = _dummy_fs_closedir,
    .create       = _dummy_fs_create,
    .unlink       = _dummy_fs_unlink,
    .mkdir        = _dummy_fs_mkdir,
    .rmdir        = _dummy_fs_rmdir,
    .stat         = _dummy_fs_stat,
    .truncate     = _dummy_fs_truncate,
};
