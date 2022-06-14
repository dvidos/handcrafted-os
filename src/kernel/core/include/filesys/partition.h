#ifndef _PARTITIONS_H
#define _PARTITIONS_H

#include <stdbool.h>
#include <devices/storage_dev.h>


struct partition {
    char *name;
    uint8_t part_no;
    struct storage_dev *dev;
    uint32_t first_sector;
    uint32_t num_sectors;
    bool bootable;
    struct partition *next;
};

void discover_storage_dev_partitions(struct storage_dev *devices_list);
struct partition *get_partitions_list();


#endif
