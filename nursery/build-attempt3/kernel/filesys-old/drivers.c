#include "../devices/storage_dev_deprecated.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "../utils/logger.h"
#include "vfs.h"
#include "drivers.h"
#include <uapi/errors.h>

MODULE("VFS", LOG_LEVEL_WARN);

static struct filesys_driver *drivers_list = NULL;


struct filesys_driver *vfs_get_drivers_list() {
    return drivers_list;
}

void vfs_register_filesys_driver(struct filesys_driver *driver) {
    // add it to the list of possible file systems (FAT, ext2 etc)
    if (drivers_list == NULL) {
        drivers_list = driver;
    } else {
        struct filesys_driver *p = drivers_list;
        while (p->next != NULL)
            p = p->next;
        p->next = driver;
    }
    driver->next = NULL;
}

struct filesys_driver *find_vfs_driver_for_partition(struct partition_deprecated *partition) {
    struct filesys_driver *driver = drivers_list;
    while (driver != NULL) {
        int result = driver->supported(partition);
        if (result == OK)
            return driver;
        driver = driver->next;
    }
    return NULL;
}
