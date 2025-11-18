#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "returns.h"
#include "sector_device.h"

typedef struct sector_device_data sector_device_data;
struct sector_device_data {
    int sector_size;
    int sector_count;
    unsigned char *data;
};

static uint32_t get_sector_size(sector_device *device) {
    return ((sector_device_data *)device->device_data)->sector_size;
}

static uint32_t get_sector_count(sector_device *device) {
    return ((sector_device_data *)device->device_data)->sector_count;
}

static int read_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    sector_device_data *sdd = (sector_device_data *)device->device_data;
    if (sector_no >= sdd->sector_count)
        return ERR_OUT_OF_BOUNDS;

    memcpy(buffer, sdd->data + (sector_no * sdd->sector_size), sdd->sector_size);
    return OK;
}

static int write_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    sector_device_data *sdd = (sector_device_data *)device->device_data;
    if (sector_no >= sdd->sector_count)
        return ERR_OUT_OF_BOUNDS;

    memcpy(sdd->data + (sector_no * sdd->sector_size), buffer, sdd->sector_size);
    return OK;
}

sector_device *new_mem_based_sector_device(int sector_size, uint32_t sector_count) {
    sector_device_data *sdd = malloc(sizeof(sector_device_data));
    sdd->sector_size = sector_size;
    sdd->sector_count = sector_count;
    sdd->data = malloc(sector_size * sector_count);

    sector_device *d = malloc(sizeof(sector_device));
    d->device_data = sdd;
    d->get_sector_count = get_sector_count;
    d->get_sector_size = get_sector_size;
    d->read_sector = read_sector;
    d->write_sector = write_sector;
    return d;
}
