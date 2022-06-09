#include <klog.h>
#include <klib/string.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <devices/storage_dev.h>


char *io_page; // 4K long


static void check_legacy_partition_table(char *sector) {
    char zeros[16];
    memset(zeros, 0, sizeof(zeros));

    // partition entries at 1BE, 1CE, 1DE, 1EE
    for (int offset = 0x1BE; offset < 0x1FE; offset += 0x10) {
        if (memcmp(&sector[offset], zeros, 16) == 0)
            continue;

        klog_debug("Found legacy partition table entry at 0x%x !!!", offset);
        klog_debug("attr %x, type %x, first sector %d, sectors %d", 
            (uint8_t)sector[offset + 0x0],
            (uint8_t)sector[offset + 0x4],
            (uint32_t)sector[offset + 0x8],
            (uint32_t)sector[offset + 0xC]
        );
    }
}

static void check_uefi_partition_table(char *sector) {
    // ...
}

static void check_storage_device(struct storage_dev *dev) {
    // see if there is a partition table
    // for each partition, see if there is a file system
    // find a way to mount each disk in a... namespace.

    klog_debug("Looking for MBR on disk %d", dev->dev_no);
    int sectors = 8;
    dev->ops.read(dev, 0, 0, sectors, io_page);
    klog_hex16_info(io_page, sectors * 512, 0);    
    check_legacy_partition_table(io_page);
    check_uefi_partition_table(io_page);
}

void init_filesys() {
    io_page = allocate_physical_page();

    // see if we have any disks
    struct storage_dev *dev = storage_mgr_get_devices_list();
    while (dev != NULL) {
        klog_debug("fs: checking storage device \"%s\"", dev->name);
        check_storage_device(dev);
        dev = dev->next;
    }
}
