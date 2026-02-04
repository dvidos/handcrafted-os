#pragma once
#include "vfs_objects/block_device.h"
#include "drivers/fs_driver_ops.h"


// these are the operations that VFS exports to the rest of the kernel


// mount management (allocates superblock_t, assigns fs_id, calls driver->mount(sb), inserts mount object into mount table)
int vfs_mount(const char *path, block_device_t *bdev, fs_driver_ops_t *driver);
int vfs_unmount(const char *path);
int vfs_sync(void);

// path resolution (walks path components, handles . / .., crosses mount points, repeatedly calls lookup())
int vfs_resolve(const char *path, file_descriptor_t *start, file_descriptor_t *out);

// file open/close (resolve path -> file_descriptor_t, allocate open_file_t, call fd->sb->driver->open(fd, flags, open_file);, store open_file_t in process FD table)
int vfs_open(const char *path, int flags, int *out_fd);
int vfs_close(int fd);

// i/o (validate FD, copy buffers if needed, call driver read/write/flush, offset lives in open_file_t)
ssize_t vfs_read(int fd, void *buf, size_t len);
ssize_t vfs_write(int fd, const void *buf, size_t len);
off_t vfs_seek(int fd, off_t off, int whence);
int vfs_flush(int fd);

// directory operations (same FD table as files, type check (must be directory), driver handles iteration state in open_file_t))
int vfs_opendir(const char *path, int *out_fd);
int vfs_readdir(int fd, struct dirent *out);
int vfs_rewinddir(int fd);
int vfs_closedir(int fd);

// metadata ops (resolve path or FD, call driver stat/truncate)
int vfs_stat(const char *path, struct stat *out);
int vfs_fstat(int fd, struct stat *out);
int vfs_truncate(const char *path, size_t size);

// creation/removal (resolve parent directory, extract final component name, call driver create/unlink/mkdir/rmdir)
int vfs_create(const char *path, int type);
int vfs_unlink(const char *path);
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
