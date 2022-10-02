#ifndef _MOUNT_H
#define _MOUNT_H

#include <devices/storage_dev.h>
#include <filesys/partition.h>

typedef struct mount_info {
    struct storage_dev *dev;
    struct partition *part;
    struct file_system_driver *driver;
    char *path;
    void *driver_private_data;
    struct file_ops *ops;
    struct mount_info *next;
} mount_info_t;

struct mount_info *vfs_get_mounts_list();
int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path);
int vfs_umount(char *path);
void vfs_discover_and_mount_filesystems(struct partition *partitions_list);



#endif
