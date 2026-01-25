#include <devices/storage_dev.h>
#include <lock.h>
#include <klog.h>


MODULE("STDEV");


struct storage_dev *storage_devices_list = NULL;
int next_dev_no = 1;
lock_t devices_list_lock;


void register_storage_device(struct storage_dev *dev) {
    acquire(&devices_list_lock);
    dev->dev_no = next_dev_no++;
    dev->next = NULL;
    
    if (storage_devices_list == NULL) {
        storage_devices_list = dev;
    } else {
        struct storage_dev *p = storage_devices_list;
        while (p->next != NULL)
            p = p->next;
        p->next = dev;
    }
    release(&devices_list_lock);
    klog_debug("Device \"%s\" registered as storage dev #%d", dev->name, dev->dev_no);
}

struct storage_dev *get_storage_devices_list() {
    return storage_devices_list;
}

struct storage_dev *get_storage_device(int dev_no) {
    struct storage_dev *ptr = storage_devices_list;
    while (ptr != NULL) {
        if (ptr->dev_no == dev_no)
            return ptr;
        ptr = ptr->next;
    }
    return NULL;
}

