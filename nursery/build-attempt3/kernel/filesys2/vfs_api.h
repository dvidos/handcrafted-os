#pragma once
#include "vfs_objects/block_device.h"
#include "fs_drivers/fs_driver_ops.h"


// below are the operations that VFS exports to the rest of the kernel
// --------------------------------------------------------------------


// mount management (allocates superblock_t, assigns fs_id, calls driver->mount(sb), inserts mount object into mount table)
int vfs2_mount(const char *path, block_device_t *dev, fs_driver_ops_t *driver);
int vfs2_unmount(const char *path);
int vfs2_sync(void);

// file open/close (resolve path -> file_descriptor_t, allocate open_file_t, call fd->sb->driver->open(fd, flags, open_file);, store open_file_t in process FD table)
int vfs2_open(const char *path, int flags, open_file_t **file);
int vfs2_close(open_file_t *file);

// i/o (validate FD, copy buffers if needed, call driver read/write/flush, offset lives in open_file_t)
ssize_t vfs2_read(open_file_t *file, void *buf, size_t len);
ssize_t vfs2_write(open_file_t *file, const void *buf, size_t len);
off_t vfs2_seek(open_file_t *file, off_t off, int whence);
int vfs2_flush(open_file_t *file);

// directory operations (same FD table as files, type check (must be directory), driver handles iteration state in open_file_t))
int vfs2_opendir(const char *path, open_file_t **dir);
int vfs2_readdir(open_file_t *dir, struct dirent *out);
int vfs2_rewinddir(open_file_t *dir);
int vfs2_closedir(open_file_t *dir);

// metadata ops (resolve path or FD, call driver stat/truncate)
int vfs2_stat(const char *path, struct stat *out);
int vfs2_fstat(open_file_t *file, struct stat *out);
int vfs2_truncate(const char *path, size_t size);

// creation/removal (resolve parent directory, extract final component name, call driver create/unlink/mkdir/rmdir)
int vfs2_create(const char *path, int type);
int vfs2_unlink(const char *path);
int vfs2_mkdir(const char *path);
int vfs2_rmdir(const char *path);



// vfs holds the mount table, does not cache things.
// it puts the FD in the process file descriptor table (???)


// initial probing of storage devices is outside of VFS
void fs_register(fs_driver_ops_t *drv);
fs_driver_ops_t *fs_probe(block_device_t *dev);
int fs_mkfs(block_device_t *dev, fs_driver_ops_t *ops);
