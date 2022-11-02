#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "objects.h"


struct ide_private_data {
    int controller_no;
    int slave_bit;
};

static int ide_disk_read(struct disk *self, int sector, char *buffer) {
    struct ide_private_data *pd = (struct ide_private_data *)self->private_data;
    return -1;
}

static int ide_disk_write(struct disk *self, int sector, char *buffer) {
    struct ide_private_data *pd = (struct ide_private_data *)self->private_data;
    return -1;
}

static struct disk_operations ide_operations = {
    .read = ide_disk_read,
    .write = ide_disk_write
};

static void ide_disk_constructor(void *instance, va_list args) {
    struct disk *disk = (struct disk *)instance;
    disk->sector_size = 512;
    disk->ops = &ide_operations;

    struct ide_private_data *pd = malloc(sizeof(struct ide_private_data));
    pd->controller_no = va_arg(args, int);
    pd->slave_bit = va_arg(args, int);
    disk->private_data = pd;
}

static void ide_disk_destructor(void *instance) {
    struct disk *disk = (struct disk *)instance;
    struct ide_private_data *pd = (struct ide_private_data *)disk->private_data;

    free(pd);
}

static struct object_info _ide_disk_info = {
    .size = sizeof(struct disk),
    .constructor = ide_disk_constructor,
    .destructor = ide_disk_destructor
};

struct object_info *IdeDisk = &_ide_disk_info;  // the only exported symbol


