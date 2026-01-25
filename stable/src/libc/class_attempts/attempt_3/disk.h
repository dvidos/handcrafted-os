#ifndef _DISK_H
#define _DISK_H

#include "objects.h"


// similar to a base or abstract class, this sets up the public contract
// there are no child classes defined, actually, we only have a strategy-type pattern applied
struct disk {
    struct class_info *class_info;

    struct disk_operations {
        int (*read)(struct disk *self, int sector, char *buffer);
        int (*write)(struct disk *self, int sector, char *buffer);
    } *ops;

    int sector_size;
    void *private_data;
};

// these are the different class contracts, but 
// the caller must know what to cast the object to (e.g. "struct disk")
extern struct class_info *IdeDisk;
extern struct class_info *RamDisk;
extern struct class_info *UsbDisk;


#endif
