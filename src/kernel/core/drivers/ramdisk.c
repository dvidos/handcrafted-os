#include <ctypes.h>
#include <errors.h>
#include <string.h>
#include <memory/virtmem.h>
#include <memory/kheap.h>
#include <devices/storage_dev.h>


struct ramdisk_info {

    void *address;
    size_t size; 

} ramdisk_info;

#define SECTOR_SIZE   512



static int ramdisk_sector_size(struct storage_dev *dev) {
    return SECTOR_SIZE;
}

static int ramdisk_read(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    if (sector_low * SECTOR_SIZE >= ramdisk_info.size)
        return ERR_BAD_ARGUMENT;
    
    void *src = ramdisk_info.address + sector_low * SECTOR_SIZE;
    memcpy(buffer, src, SECTOR_SIZE);
    return SUCCESS;
}

static int ramdisk_write(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    if (sector_low * SECTOR_SIZE >= ramdisk_info.size)
        return ERR_BAD_ARGUMENT;
    
    void *dst = ramdisk_info.address + sector_low * SECTOR_SIZE;
    memcpy(dst, buffer, SECTOR_SIZE);
    return SUCCESS;
}

// creates a ram disk of some size and registers with device manager
// if instead of memory mapped content, we had a file mapped content,
// we would have invented the loopback device!
void init_ramdisk(size_t size) {

    // we must have a way to get kernel's current addresses
    // and a way to maintain the kernel's resident allocated mappings
    // let's go for the 10MB+ area for now.
    ramdisk_info.address = (void *)0xA00000; // 10MB
    ramdisk_info.size = size;
    allocate_virtual_memory_range(
        ramdisk_info.address, 
        ramdisk_info.address + ramdisk_info.size, 
        get_kernel_page_directory()
    );

    memset(ramdisk_info.address, 0, ramdisk_info.size);

    struct storage_dev_ops *ops = kmalloc(sizeof(struct storage_dev_ops));
    ops->sector_size = ramdisk_sector_size;
    ops->read = ramdisk_read;
    ops->write = ramdisk_write;

    struct storage_dev *ramdisk_dev = kmalloc(sizeof(struct storage_dev));
    memset(ramdisk_dev, 0, sizeof(struct storage_dev));
    ramdisk_dev->name = "RAM disk";
    ramdisk_dev->ops = ops;
    ramdisk_dev->driver_priv_data = &ramdisk_info;
    
    storage_mgr_register_device(ramdisk_dev);
}


