#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "objects.h"



struct usb_private_data {
    char *device_name;
};

static int usb_disk_read(struct disk *self, int sector, char *buffer) {
    struct usb_private_data *pd = (struct usb_private_data *)self->private_data;
    return -1;
}

static int usb_disk_write(struct disk *self, int sector, char *buffer) {
    struct usb_private_data *pd = (struct usb_private_data *)self->private_data;
    return -1;
}

static struct disk_operations usb_operations = {
    .read = usb_disk_read,
    .write = usb_disk_write
};

static void usb_disk_constructor(void *instance, va_list args) {
    struct disk *disk = (struct disk *)instance;
    disk->class_info->name = "Disk_USB";
    disk->sector_size = 512;
    disk->ops = &usb_operations;

    struct usb_private_data *pd = malloc(sizeof(struct usb_private_data));
    pd->device_name = strdup(va_arg(args, char *));
}

static void usb_disk_destructor(void *instance) {
    struct disk *disk = (struct disk *)instance;
    struct usb_private_data *pd = (struct usb_private_data *)disk->private_data;
    free(pd->device_name);
    free(pd);
}

static struct class_info _usb_disk_info = {
    .size = sizeof(struct disk),
    .constructor = usb_disk_constructor,
    .destructor = usb_disk_destructor
};

struct class_info *UsbDisk = &_usb_disk_info;  // the only exported symbol

