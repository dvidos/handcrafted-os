#include "../fs_driver_ops.h"
#include "internal.h"
#include "../../../include/uapi/errors.h"


int _skeleton_fs_probe(block_device_t *dev) {
    // just check if device seems to contain a supported filesystem
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_mount(superblock_t *sb) {
    // create private data, store on sb->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_unmount(superblock_t *sb) {
    // destroy private data from sb->driver_priv_data
    return ERR_NOT_IMPLEMENTED;
}

int _skeleton_fs_sync(superblock_t *sb) {
    return ERR_NOT_IMPLEMENTED;
}

