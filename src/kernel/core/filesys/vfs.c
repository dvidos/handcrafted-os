#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <klog.h>


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
                break;
            }
            driver = driver->next;
        }

        partition = partition->next;
    }

    // keep in mind that floppies some other storage media (e.g. usb sticks)
    // may have a file system without partitions
}



/*

// essentially, VFS (virtual file system) is the general overall filesystem
// the other "file systems" refer to the discrete filesystems: FAT, ext2 etc.
struct file_system_ops;

struct mount_info {
    struct storage_dev *dev;
    char *path;
    void *driver_private_data;
    struct file_system_ops *ops;
};

struct mount_info *mounted_filesystems_list = NULL;


// operations of a logical file system
struct file_system_ops {
    int (*stat)();

    int (*opendir)();
    int (*readdir)();
    int (*closedir)();

    int (*open)();
    int (*read)();
    int (*write)();
    int (*close)();

    int (*touch)();
    int (*mkdir)();
    int (*unlink)();
};


// functions to be offered by the filesystem manager.
int vfs_mount(struct storage_dev *dev, char *path) {
    // iterate over drivers to find what file system this is
    // then link it to the corresponding path
    // and add it to list of mounted file systems
    // the driver will give us an `ops` struct for us to use
}
int vfs_umount(char *path) {
    // remove from list of mounted file systems
}
int vfs_opendir(char *path) {

}
int vfs_stat(char *path) {

}
int vfs_readdir() {

}
int vfs_closedir() {

}
int vfs_open(char *path) {

}
int vfs_read() {

}
int vfs_write() {

}
int vfs_close() {

}
int vfs_ioctl() {

}
int vfs_touch(char *path) {

}
int vfs_mkdir(char *path) {

}
int vfs_unlink(char *path) {

}
*/