#ifndef _MOUNT_H
#define _MOUNT_H

#include <devices/storage_dev.h>
#include <filesys/partition.h>

typedef struct mount_info {
    struct storage_dev *dev;
    struct partition *part;
    struct filesys_driver *driver;
    struct superblock *superblock;
    char *mount_point;
    file_t *root_dir;

    // maybe we only need the following pair:
    file_descriptor_t *host_dir;        // NULL for root mount
    file_descriptor_t *mounted_fs_root; // root dir of mounted fs

    struct mount_info *next;
} mount_info_t;

struct mount_info *vfs_get_mounts_list();
struct mount_info *vfs_get_root_mount();
struct mount_info *vfs_get_mount_info_by_numbers(int dev_no, int part_no);
struct mount_info *vfs_get_mount_info_by_path(char *path);

int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path);
int vfs_umount(char *path);
int vfs_discover_and_mount_filesystems(char *kernel_cmd_line);



#endif
