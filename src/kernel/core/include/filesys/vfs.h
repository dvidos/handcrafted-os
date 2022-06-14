#ifndef _VFS_H
#define _VFS_H

#include <devices/storage_dev.h>
#include <filesys/partition.h>


struct file_system_driver {
    char *name; // e.g. "FAT", "ext2" etc.
    struct file_system_driver *next;

    // looks at a storage dev to see if the filesystem can be understood 
    // if so, it calls register_mountable_file_system() to register it
    int (*probe)(struct partition *partition);
};

void vfs_register_file_system_driver(struct file_system_driver *driver);
void vfs_discover_and_mount_filesystems(struct partition *partitions_list);


struct mount_info {
    struct storage_dev *dev;
    struct partition *part;
    char *path;
    void *driver_private_data;
    struct file_system_ops *ops;
    struct mount_info *next;
};

struct mount_info *vfs_get_mounts_list();
int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path);
int vfs_umount(char *path);


#endif
