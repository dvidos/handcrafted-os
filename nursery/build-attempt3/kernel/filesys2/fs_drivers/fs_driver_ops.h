#pragma once

// this file contains the contract that drivers must implement,
// in order to participate in the VFS system

#include "../../include/uapi/vfs2_file_flags.h"
#include "../../include/uapi/vfs2_stat.h"
#include "../../include/uapi/vfs2_dirent.h"
#include "../vfs_objects/block_device.h"
#include "../vfs_objects/superblock.h"
#include "../vfs_objects/file_descriptor.h"
#include "../vfs_objects/open_file.h"


typedef struct fs_driver_ops   fs_driver_ops_t;

// this is all a driver needs to support
struct fs_driver_ops {
    int (*probe)(block_device_t *dev);
    int (*mount)(superblock_t *sb);
    int (*unmount)(superblock_t *sb);
    int (*sync)(superblock_t *sb);

    int (*get_root_dir)(superblock_t *sb, file_descriptor_t **out);
    int (*lookup)(file_descriptor_t *dir, const char *name, file_descriptor_t **out);

    int (*open)(file_descriptor_t *fd, int flags, open_file_t **file_handle);
    int (*close)(open_file_t *file);
    int (*read)(open_file_t *file, void *buf, size_t len, off_t offset);
    int (*write)(open_file_t *file, const void *buf, size_t len, off_t offset);
    int (*flush)(open_file_t *file);

    int (*opendir)(file_descriptor_t *dir, open_file_t **dir_handle);
    int (*readdir)(open_file_t *dir_handle, struct dirent *out);
    int (*rewinddir)(open_file_t *dir_handle);
    int (*closedir)(open_file_t *dir_handle);

    int (*create)(file_descriptor_t *parent, const char *name, int type, file_descriptor_t **out);
    int (*unlink)(file_descriptor_t *parent, const char *name);
    int (*mkdir)(file_descriptor_t *parent, const char *name); // dirs have special create semantics
    int (*rmdir)(file_descriptor_t *parent, const char *name); // dirs have special delete semantics

    int (*stat)(file_descriptor_t *fd, struct stat *out);
    int (*truncate)(file_descriptor_t *fd, size_t size);
};

