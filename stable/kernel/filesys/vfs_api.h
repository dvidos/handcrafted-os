#pragma once
#include "../devices/block/block_device.h"
#include "vfs_objects/mount_table.h"
#include "fs_driver.h"


typedef struct vfs_context vfs_context_t;
struct vfs_context {
    mount_table_t *mtab;
    inode_t root_inode;
    inode_t cwd_inode;
    uint16_t creation_mask;  // e.g. 0022
    uint8_t uid;
    uint8_t gid;
};



// mount management (allocates superblock_t, assigns fs_id, calls driver->mount(sb), inserts mount object into mount table)
error_t vfs_mount(vfs_context_t *ctx, const char *path, block_device_t *dev, fs_driver_ops_t *driver);
inode_t vfs_root_inode(vfs_context_t *ctx);
error_t vfs_sync(vfs_context_t *ctx);
error_t vfs_unmount(vfs_context_t *ctx, const char *path);

// needed for chdir()
error_t vfs_lookup(vfs_context_t *ctx, const char *path, inode_t *target_out);
void    vfs_canonicalize(char *path);

// file open/close (resolve path -> inode_t, allocate open_file_t, call n->sb->driver->open(n, flags, open_file);, store open_file_t in process FD table)
error_t vfs_open(vfs_context_t *ctx, const char *path, int flags, open_file_t **file);
error_t vfs_close(open_file_t *file);

// i/o (validate FD, copy buffers if needed, call driver read/write/flush, offset lives in open_file_t)
ssize_t vfs_read(open_file_t *file, void *buf, size_t len);
ssize_t vfs_write(open_file_t *file, const void *buf, size_t len);
off_t vfs_seek(open_file_t *file, off_t off, int whence);
error_t vfs_flush(open_file_t *file);

// directory operations (same FD table as files, type check (must be directory), driver handles iteration state in open_file_t))
error_t vfs_opendir(vfs_context_t *ctx, const char *path, open_file_t **dir);
ssize_t vfs_readdir(open_file_t *dir, vfs_dirent_t *out);
error_t vfs_rewinddir(open_file_t *dir);
error_t vfs_closedir(open_file_t *dir);

// metadata ops (resolve path or FD, call driver stat/truncate)
error_t vfs_stat(vfs_context_t *ctx, const char *path, vfs_stat_t *out);
error_t vfs_fstat(open_file_t *file, vfs_stat_t *out);
error_t vfs_access(vfs_context_t *ctx, const char *path, int mode);
error_t vfs_chmod(vfs_context_t *ctx, const char *path, uint32_t mode);
error_t vfs_fchmod(vfs_context_t *ctx, open_file_t *file, uint32_t mode);
error_t vfs_chown(vfs_context_t *ctx, const char *path, uid_t uid, gid_t gid);
error_t vfs_fchown(vfs_context_t *ctx, open_file_t *file, uid_t uid, gid_t gid);
error_t vfs_truncate(vfs_context_t *ctx, const char *path, size_t size);
error_t vfs_ioctl(open_file_t *file, uint32_t cmd, long arg);

error_t vfs_permission(vfs_context_t *ctx, inode_t *n, int mode);

// creation/removal (resolve parent directory, extract final component name, call driver create/unlink/mkdir/rmdir)
error_t vfs_create(vfs_context_t *ctx, const char *path, int type);
error_t vfs_unlink(vfs_context_t *ctx, const char *path);
error_t vfs_mkdir(vfs_context_t *ctx, const char *path);
error_t vfs_rmdir(vfs_context_t *ctx, const char *path);


// vfs holds the mount table, does not cache things.
// it puts the FD in the process file descriptor table (???)
// we don't want to merge proc and fs, same as curr-dir, we need to see how.

