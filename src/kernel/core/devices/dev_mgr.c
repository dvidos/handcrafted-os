#include <stddef.h>
#include <stdint.h>
#include "../memory/kheap.h"
#include "../string.h"


struct device_entry {
    struct device_entry *next;

    char *device_type; // name for the particular device, e.g. "tty"
    uint8_t device_no;  // number of device in the family - denotes instance to use

    void *device;
};
typedef struct device_entry device_entry_t;

device_entry_t *devices_list = NULL;


// some device driver code (e.g. sata) will create
// a device structure (with device nums, operations, data)
// and register it here.
// data can be anything the driver wants. 
// it will be passed as the first argument to every operation function.
// for example, it may contain a semaphore or mutex.
// the device driver is free to use kmalloc() and kfree() as needed.
// it seems that drivers, as running on kernel space, need special functions
//     to access user-level pointers. maybe this was what virt_to_phys() was doing in minix
//     in linux these are copy_from_user() and copy_to_user()
//     that's how, say, a loop device would copy data from its buffer to the read() buffer.

void devmgr_register_device(char *device_type, char device_no, void *device) {
    // we add this device to the tree
    device_entry_t *entry = kmalloc(sizeof(device_entry_t));
    entry->device_type = device_type;
    entry->device_no = device_no;

    // inset in list
    entry->next = devices_list;
    devices_list = entry;
}

void *devmgr_get_device(char *device_type, char device_no) {
    device_entry_t *entry = devices_list;
    while (entry != NULL) {
        if (strcmp(entry->device_type, device_type) == 0 && entry->device_no == device_no)
            return entry;
        entry = entry->next;
    }

    return NULL;
}

