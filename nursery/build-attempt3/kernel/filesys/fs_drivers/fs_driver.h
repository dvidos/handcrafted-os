#pragma once

// this file contains the contract that drivers must implement,
// in order to participate in the VFS system

#include "../../include/uapi/errors.h"
#include "../../include/uapi/vfs_mount_flags.h"
#include "../../include/uapi/vfs_seek_flags.h"
#include "../../include/uapi/vfs_file_flags.h"
#include "../../include/uapi/vfs_stat.h"
#include "../../include/uapi/vfs_dirent.h"
#include "../../devices/block/block_device.h"
#include "../vfs_objects/superblock.h"
#include "../vfs_objects/inode.h"
#include "../vfs_objects/open_file.h"


typedef struct fs_driver       fs_driver_t;
typedef struct fs_driver_ops   fs_driver_ops_t;

// this is all a driver needs to support
struct fs_driver_ops {
    error_t (*probe)(block_device_t *dev);
    error_t (*mount)(superblock_t *sb);
    error_t (*unmount)(superblock_t *sb);
    error_t (*sync)(superblock_t *sb);
    error_t (*mkfs)(block_device_t *dev);
    
    error_t (*get_root_dir)(superblock_t *sb, inode_t **out);
    error_t (*lookup)(inode_t *dir, const char *name, inode_t **out);

    error_t (*open)(inode_t *n, int flags, open_file_t **file_handle);
    error_t (*close)(open_file_t *file);
    ssize_t (*read)(open_file_t *file, void *buf, size_t len, off_t offset); // return bytes read
    ssize_t (*write)(open_file_t *file, const void *buf, size_t len, off_t offset); // return bytes written
    error_t (*flush)(open_file_t *file);

    error_t (*opendir)(inode_t *dir, open_file_t **dir_handle);
    ssize_t (*readdir)(open_file_t *dir_handle, vfs_dirent_t *out); // return bytes read
    error_t (*rewinddir)(open_file_t *dir_handle);
    error_t (*closedir)(open_file_t *dir_handle);

    error_t (*create)(inode_t *parent, const char *name, int type, inode_t **out);
    error_t (*unlink)(inode_t *parent, const char *name);
    error_t (*mkdir)(inode_t *parent, const char *name, inode_t **out); // dirs have special create semantics
    error_t (*rmdir)(inode_t *parent, const char *name); // dirs have special delete semantics

    error_t (*stat)(inode_t *n, vfs_stat_t *out);
    error_t (*truncate)(inode_t *n, size_t size);
};

struct fs_driver {
    const char *name;
    fs_driver_ops_t *ops;
    error_t (*probe)(block_device_t *);
    error_t (*mkfs)(block_device_t *);

    struct fs_driver *next;
};
