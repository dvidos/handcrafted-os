#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <filesys/drivers.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>

MODULE("VFS");

static struct file_system_driver *drivers_list = NULL;


struct file_system_driver *vfs_get_drivers_list() {
    return drivers_list;
}

void vfs_register_file_system_driver(struct file_system_driver *driver) {
    // add it to the list of possible file systems (FAT, ext2 etc)
    if (drivers_list == NULL) {
        drivers_list = driver;
    } else {
        struct file_system_driver *p = drivers_list;
        while (p->next != NULL)
            p = p->next;
        p->next = driver;
    }
    driver->next = NULL;
}

struct file_system_driver *find_driver_for_partition(struct partition *partition) {
    struct file_system_driver *driver = drivers_list;
    while (driver != NULL) {
        int err = driver->probe(partition);
        if (!err)
            return driver;
        driver = driver->next;
    }
    return NULL;
}
