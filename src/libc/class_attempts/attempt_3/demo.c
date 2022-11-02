#include "objects.h"
#include "disk.h"


void demo() {
    char buffer[512];

    struct disk *disk1 = new(IdeDisk, 0, 0);
    struct disk *disk2 = new(RamDisk, 1024);
    struct disk *disk3 = new(UsbDisk, "/dev/usb1");

    if (disk1->ops->read(disk1, 1, buffer))
        disk2->ops->write(disk2, 24, buffer);

    delete(disk1, disk1->object_info);
    delete(disk2, disk2->object_info);
    delete(disk3, disk2->object_info);
}