#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <filesys/drivers.h>
#include <filesys/mount.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>

MODULE("VFS");

static struct mount_info *mounts_list;
static void add_mount_info_to_list(struct mount_info *info);



struct mount_info *vfs_get_mounts_list() {
    return mounts_list;
}

int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path) {
    klog_trace("vfs_mount(%d, %d, \"%s\")", dev_no, part_no, path);

    struct storage_dev *dev = storage_mgr_get_device(dev_no);
    if (dev == NULL)
        return ERR_NO_DEVICE;
    struct partition *part = get_partition(dev, part_no);
    if (part == NULL)
        return ERR_NO_PARTITION;
    struct file_system_driver *driver = find_driver_for_partition(part);
    if (driver == NULL)
        return ERR_NO_DRIVER_FOUND;

    // so now we can mount?
    struct mount_info *mount = kmalloc(sizeof(struct mount_info));
    mount->dev = dev;
    mount->part = part;
    mount->driver = driver;
    mount->ops = driver->get_file_operations();
    mount->driver_private_data = NULL;
    mount->path = kmalloc(strlen(path) + 1);
    strcpy(mount->path, path);  

    add_mount_info_to_list(mount);

    return NO_ERROR;
}

static void add_mount_info_to_list(struct mount_info *info) {
    if (mounts_list == NULL) {
        mounts_list = info;
    } else {
        struct mount_info *p = mounts_list;
        while (p->next != NULL)
            p = p->next;
        p->next = info;
    }
    info->next = NULL;
}


int vfs_umount(char *path) {
    klog_trace("vfs_umount(\"%s\")", path);
    // remember to free mount->path as well
    return ERR_NOT_IMPLEMENTED;
}

void vfs_discover_and_mount_filesystems(struct partition *partitions_list) {
    klog_debug("Discovering and mounting file systems");
    struct partition *partition = partitions_list;
    while (partition != NULL) {
        struct file_system_driver *driver = find_driver_for_partition(partition);
        // and? which partition do we mount as root? where do we mount them?
        if (driver != NULL) {
            char name[10];
            sprintfn(name, sizeof(name), "/d%dp%d", partition->dev->dev_no, partition->part_no);
            
            vfs_mount(partition->dev->dev_no, partition->part_no, name);
        }
        partition = partition->next;
    }

    // keep in mind that floppies some other storage media (e.g. usb sticks)
    // may have a file system without partitions
    // so maybe we should name partitions "logical_vol"
}
