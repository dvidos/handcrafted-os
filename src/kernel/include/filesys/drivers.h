#ifndef _DRIVERS_H
#define _DRIVERS_H

#include <devices/storage_dev.h>

struct partition;
struct superblock;

struct filesys_driver {
    struct filesys_driver *next;
    
    char *name; // e.g. "FAT", "ext2" etc.

    // supported() looks at a storage dev to see if the filesystem can be understood 
    int (*supported)(struct partition *partition);
    
    // used in (un)mount operations
    int (*open_superblock)(struct partition *partition, struct superblock *superblock);
    int (*close_superblock)(struct superblock *superblock);
};


struct filesys_driver *vfs_get_drivers_list();
void vfs_register_filesys_driver(struct filesys_driver *driver);
struct filesys_driver *find_vfs_driver_for_partition(struct partition *partition);



#endif
