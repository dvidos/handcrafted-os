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
    struct file_ops *(*get_file_operations)();
};

void vfs_register_file_system_driver(struct file_system_driver *driver);
void vfs_discover_and_mount_filesystems(struct partition *partitions_list);



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

typedef struct dir_entry {
    char short_name[12+1];
    char file_size;
} dir_entry_t;

struct file_ops;

typedef struct file {
    struct storage_dev *storage_dev;
    struct partition *partition;
    struct file_system_driver *driver;
    
    char *path; // relative to mount point
    void *driver_priv_data;
    struct file_ops *ops;
} file_t;

struct file_ops {
    int (*opendir)(char *path, file_t *file);
    int (*readdir)(file_t *file, dir_entry_t *entry);
    int (*closedir)(file_t *file);

    int (*open)(char *path, file_t *file);
    int (*read)(file_t *file, char *buffer, int bytes);
    int (*close)(file_t *file);
};

int vfs_opendir(char *path, file_t *file);
int vfs_readdir(file_t *file, struct dir_entry *dir_entry);
int vfs_closedir(file_t *file);
int vfs_open(char *path, file_t *file);
int vfs_read(file_t *file, char *buffer, int bytes);
int vfs_close(file_t *file);




#endif
