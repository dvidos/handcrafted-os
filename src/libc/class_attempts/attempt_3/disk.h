#ifndef _DISK_H
#define _DISK_H

#include "objects.h"


// similar to a base or abstract class, this sets up the public contract
struct disk {
    struct object_info *object_info;

    struct disk_operations {
        int (*read)(struct disk *self, int sector, char *buffer);
        int (*write)(struct disk *self, int sector, char *buffer);
    } *ops;

    int sector_size;
    void *private_data;
};

extern struct object_info *IdeDisk;
extern struct object_info *RamDisk;
extern struct object_info *UsbDisk;


#endif
