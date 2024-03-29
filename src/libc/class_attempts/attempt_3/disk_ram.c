#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "objects.h"


struct ram_private_data {
    void *buffer;
    int disk_size;
};

static int ram_disk_read(struct disk *self, int sector, char *buffer) {
    struct ram_private_data *pd = (struct ram_private_data *)self->private_data;
    // validations are omitted for brevity
    memcpy(buffer, pd->buffer + (512 * sector), 512);
    return 0;
}

static int ram_disk_write(struct disk *self, int sector, char *buffer) {
    struct ram_private_data *pd = (struct ram_private_data *)self->private_data;
    // validations are omitted for brevity
    memcpy(pd->buffer + (512 * sector), buffer, 512);
    return 0;
}

static struct disk_operations ram_operations = {
    .read = ram_disk_read,
    .write = ram_disk_write
};

static void ram_disk_constructor(void *instance, va_list args) {
    struct disk *disk = (struct disk *)instance;
    disk->class_info->name = "Disk_RAM";
    disk->sector_size = 512;
    disk->ops = &ram_operations;

    struct ram_private_data *p = malloc(sizeof(struct ram_private_data));
    p->disk_size = va_arg(args, int);
    p->buffer = malloc(p->disk_size);
    memset(p->buffer, 0, p->disk_size);
}

static void ram_disk_destructor(void *instance) {
    struct disk *disk = (struct disk *)instance;
    struct ram_private_data *pd = (struct ram_private_data *)disk->private_data;
    free(pd->buffer);
    free(pd);
}

static struct class_info _ram_disk_info = {
    .size = sizeof(struct disk),
    .constructor = ram_disk_constructor
};

struct class_info *RamDisk = &_ram_disk_info;  // the only exported symbol


