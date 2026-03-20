#include "fs_api.h"
#include "vfs_api.h"
#include "../include/uapi/errors.h"
#include "../include/uapi/vfs_seek_flags.h"
#include "../include/uapi/vfs_dirent.h"
#include "fs_drivers/fs_driver.h"
#include "vfs_objects/superblock.h"
#include "vfs_objects/inode.h"
#include "vfs_objects/open_file.h"
#include "vfs_objects/mount_table.h"
#include "../klib/path.h"
#include "../klib/string.h"


static fs_driver_t *registered_drivers_list = 0;
static device_t *registered_devices_list = 0;



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


void fs_register_device(const char *name, dev_driver_t *drv, int dev_number) {
    device_t *dev = kmalloc(sizeof(device_t));
    memset(dev, 0, sizeof(device_t));

    dev->name = kstrdup(name);
    dev->driver = drv;
    dev->dev_number = dev_number;
    
    if (registered_devices_list == 0) {
        registered_devices_list = dev;
    } else {
        device_t *prev = registered_devices_list;
        while (prev->next != NULL)
            prev = prev->next;
        prev->next = dev;
    }
    dev->next = NULL;
}

const device_t *fs_lookup_device(const char *name) {
    for (device_t *d = registered_devices_list; d != NULL; d = d->next) {
        if (strcmp(d->name, name) == 0)
            return d;
    }

    return NULL;
}
