#ifndef _STORAGE_DEV_H
#define _STORAGE_DEV_H

#include <ctypes.h>
#include <drivers/pci.h>

struct storage_dev_ops;

struct storage_dev {
    char dev_no;            // given by device manager
    
    char *name;             // prepared by driver, human readable
    pci_device_t *pci_dev;  // possible pci device
    struct storage_dev_ops *ops;  // operations for use by file system
    void *driver_priv_data;        // for private use of driver

    struct storage_dev *next;    // next device in list
};

struct storage_dev_ops {
    int (*sector_size)(struct storage_dev *dev);
    int (*read)(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer);
    int (*write)(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer);
};

void register_storage_device(struct storage_dev *dev);
struct storage_dev *get_storage_devices_list();
struct storage_dev *get_storage_device(int dev_no);



#endif
