#pragma once
#include <ctypes.h>
#include "../drivers/fs_driver_ops.h"
#include "../../include/uapi/vfs2_mount_flags.h"


typedef struct mount_point {
    // host filesystem
    file_descriptor_t *host_dir;

    // mounted filesystem
    superblock_t *sb;
    file_descriptor_t *root_dir;

    // options
    uint32_t flags;  // see VFS_MOUNT_*
    int ref_count;

} mount_point_t;
