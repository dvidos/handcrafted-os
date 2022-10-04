#ifndef _DRIVERS_H
#define _DRIVERS_H

#include <devices/storage_dev.h>

struct partition;
struct superblock;

struct file_system_driver {
    struct file_system_driver *next;
    
    char *name; // e.g. "FAT", "ext2" etc.

    // supported() looks at a storage dev to see if the filesystem can be understood 
    int (*supported)(struct partition *partition);
    
    // used in (un)mount operations
    int (*open_superblock)(struct partition *partition, struct superblock *superblock);
    int (*close_superblock)(struct superblock *superblock);
};


struct file_system_driver *vfs_get_drivers_list();
void vfs_register_file_system_driver(struct file_system_driver *driver);
struct file_system_driver *find_vfs_driver_for_partition(struct partition *partition);



#endif
