#pragma once

// this file contains the contract that drivers must implement,
// in order to participate in the VFS system

#include "../include/uapi/errors.h"
#include "../include/uapi/vfs_mount_flags.h"
#include "../include/uapi/vfs_seek_flags.h"
#include "../include/uapi/vfs_file_flags.h"
#include "../include/uapi/vfs_stat.h"
#include "../include/uapi/vfs_dirent.h"
#include "../devices/block/block_device.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/inode.h"
#include "vfs_objects/open_file.h"
#include "fs_driver.h"


typedef struct device           device_t;
typedef struct dev_driver       dev_driver_t;


// all device drivers (ttys, serial ports, etc should expose one instance)
struct dev_driver {
    const char *name;
    fs_driver_ops_t *ops;  // note this has the same fs_driver_ops as file systems for VFS transparency

    struct fs_driver *next;
};


// all devices registered use this structure
struct device {
    const char *name;
    dev_driver_t *driver;
    int dev_number;
    bool is_stream;

    struct device *next;
};
