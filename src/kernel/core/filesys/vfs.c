#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <memory/kheap.h>
#include <klog.h>
#include <errors.h>


static struct mount_info *mounts_list;


static struct file_system_driver *drivers_list = NULL;

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

void vfs_discover_and_mount_filesystems(struct partition *partitions_list) {
    klog_debug("Discovering and mounting file systems");
    struct partition *partition = partitions_list;
    while (partition != NULL) {
        
        struct file_system_driver *driver = drivers_list;
        while (driver != NULL) {
            int err = driver->probe(partition);
            if (err == 0) {
                klog_debug("Driver %s seems to understand partition %s", driver->name, partition->name);
                // should mount this filesystem.
                break;
            }
            driver = driver->next;
        }

        partition = partition->next;
    }

    // keep in mind that floppies some other storage media (e.g. usb sticks)
    // may have a file system without partitions
    // so maybe we should name partitions "logical_vol"
}



struct mount_info *vfs_get_mounts_list() {
    return mounts_list;
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


int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path) {
    struct storage_dev *dev = storage_mgr_get_device(dev_no);
    if (dev == NULL)
        return ERR_NO_DEVICE;
    struct partition *part = get_partition(dev, part_no);
    if (part == NULL)
        return ERR_NO_PARTITION;
    
    // find a driver that works
    struct file_system_driver *p = drivers_list;
    struct file_system_driver *driver = NULL;
    while (p != NULL) {
        if (p->probe(part) == NO_ERROR) {
            driver = p;
            break;
        }
        p = p->next;
    }
    if (driver == NULL)
        return ERR_NO_DRIVER_FOUND;

    // so now we can mount?
    struct mount_info *mount = kmalloc(sizeof(struct mount_info));
    mount->dev = dev;
    mount->part = part;
    mount->driver = driver;
    mount->ops = driver->get_file_operations();
    mount->driver_private_data = NULL;
    mount->path = path;
    add_mount_info_to_list(mount);

    return NO_ERROR;
}

int vfs_umount(char *path) {
    return ERR_NOT_IMPLEMENTED;
}



static int parse_path_and_prepare_file_structure(char *path, file_t *file) {

    // in theory, we would parse the path to find the mount point and mounted path
    // but for now, let's assume only one mounted system
    struct mount_info *mount = mounts_list;
    if (mount == NULL)
        return ERR_NOT_FOUND;

    // we need to fill in the file_t structure.
    file->storage_dev = mount->dev;
    file->partition = mount->part;
    file->driver = mount->driver;
    file->ops = mount->driver->get_file_operations();

    // normally, we should give the relative path, after the mount
    file->path = path;
    
    return NO_ERROR;    
}

int vfs_opendir(char *path, file_t *file) {
    int err = parse_path_and_prepare_file_structure(path, file);
    if (err)
        return err;
    return file->ops->opendir(file->path, file);
}

int vfs_readdir(file_t *file, struct dir_entry *dir_entry) {
    return file->ops->readdir(file, dir_entry);
}

int vfs_closedir(file_t *file) {
    return file->ops->closedir(file);
}

int vfs_open(char *path, file_t *file) {
    int err = parse_path_and_prepare_file_structure(path, file);
    if (err)
        return err;
    return file->ops->open(file->path, file);
}

int vfs_read(file_t *file, char *buffer, int bytes) {
    return file->ops->read(file, buffer, bytes);
}

int vfs_close(file_t *file) {
    return file->ops->close(file);
}


