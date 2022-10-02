#ifndef _DRIVERS_H
#define _DRIVERS_H

#include <devices/storage_dev.h>
#include <filesys/partition.h>

struct file_system_driver {
    char *name; // e.g. "FAT", "ext2" etc.
    struct file_system_driver *next;

    // probe() looks at a storage dev to see if the filesystem can be understood 
    int (*probe)(struct partition *partition);
    
    // returns file operations pointers for this partition
    struct file_ops *(*get_file_operations)();
};


struct file_system_driver *vfs_get_drivers_list();
void vfs_register_file_system_driver(struct file_system_driver *driver);
struct file_system_driver *find_vfs_driver_for_partition(struct partition *partition);



#endif
