#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs2_seek_flags.h"
#include "../include/uapi/vfs2_dirent.h"
#include "fs_drivers/fs_driver.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/inode.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"


static fs_driver_t *registered_drivers_list = 0;

void fs_register(fs_driver_t *drv) {
    if (registered_drivers_list == 0) {
        registered_drivers_list = drv;
    } else {
        fs_driver_t *p = registered_drivers_list;
        while (p->next != NULL)
            p = p->next;
        p->next = drv;
    }
    drv->next = NULL;
}

fs_driver_t *fs_probe(block_device_t *dev) {
    for (fs_driver_t *drv = registered_drivers_list; drv; drv = drv->next) {
        error_t err = drv->probe(dev);
        if (err == OK) return drv;
    }

    return NULL;
}

error_t fs_mkfs(block_device_t *dev, fs_driver_t *drv) {
    return drv->mkfs(dev);
}
