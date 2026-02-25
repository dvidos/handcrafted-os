#pragma once
#include "../devices/block/block_device.h"
#include "fs_drivers/fs_driver.h"


// mount management (allocates superblock_t, assigns fs_id, calls driver->mount(sb), inserts mount object into mount table)
error_t vfs2_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver);
error_t vfs2_unmount(const char *path);
error_t vfs2_sync(void);

// file open/close (resolve path -> inode_t, allocate open_file_t, call n->sb->driver->open(n, flags, open_file);, store open_file_t in process FD table)
error_t vfs2_open(const char *path, int flags, open_file_t **file);
error_t vfs2_close(open_file_t *file);

// i/o (validate FD, copy buffers if needed, call driver read/write/flush, offset lives in open_file_t)
ssize_t vfs2_read(open_file_t *file, void *buf, size_t len);
ssize_t vfs2_write(open_file_t *file, const void *buf, size_t len);
off_t vfs2_seek(open_file_t *file, off_t off, int whence);
error_t vfs2_flush(open_file_t *file);

// directory operations (same FD table as files, type check (must be directory), driver handles iteration state in open_file_t))
error_t vfs2_opendir(const char *path, open_file_t **dir);
ssize_t vfs2_readdir(open_file_t *dir, struct dirent *out);
error_t vfs2_rewinddir(open_file_t *dir);
error_t vfs2_closedir(open_file_t *dir);

// metadata ops (resolve path or FD, call driver stat/truncate)
error_t vfs2_stat(const char *path, struct stat *out);
error_t vfs2_fstat(open_file_t *file, struct stat *out);
error_t vfs2_truncate(const char *path, size_t size);

// creation/removal (resolve parent directory, extract final component name, call driver create/unlink/mkdir/rmdir)
error_t vfs2_create(const char *path, int type);
error_t vfs2_unlink(const char *path);
error_t vfs2_mkdir(const char *path);
error_t vfs2_rmdir(const char *path);


// vfs holds the mount table, does not cache things.
// it puts the FD in the process file descriptor table (???)
// we don't want to merge proc and fs, same as curr-dir, we need to see how.

