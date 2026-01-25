#include "partition_sector_device.h"
#include <stdio.h>
#include <stdlib.h>
#include "../cli_tool/utils.h" // For error function

typedef struct {
    sector_device *parent;
    uint32_t start_sector;
    uint32_t sector_count;
} partition_device_data;

static uint32_t partition_get_sector_size(sector_device *device) {
    partition_device_data *data = (partition_device_data *)device->device_data;
    return data->parent->get_sector_size(data->parent);
}

static uint32_t partition_get_sector_count(sector_device *device) {
    partition_device_data *data = (partition_device_data *)device->device_data;
    return data->sector_count;
}

static int partition_read_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    partition_device_data *data = (partition_device_data *)device->device_data;
    if (sector_no >= data->sector_count) {
        // error("Attempt to read out of bounds sector %u (max %u) from partition.", sector_no, data->sector_count); // Cannot use error here.
        return -1; // Out of bounds
    }
    return data->parent->read_sector(data->parent, data->start_sector + sector_no, buffer);
}

static int partition_write_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    partition_device_data *data = (partition_device_data *)device->device_data;
    if (sector_no >= data->sector_count) {
        // error("Attempt to write out of bounds sector %u (max %u) to partition.", sector_no, data->sector_count); // Cannot use error here.
        return -1; // Out of bounds
    }
    return data->parent->write_sector(data->parent, data->start_sector + sector_no, buffer);
}

static void partition_dump_debug_info(sector_device *device, const char *title) {
    // Implementation can be added if needed
}

sector_device *new_partition_sector_device(sector_device *parent, uint32_t start_sector, uint32_t sector_count) {
    if (!parent) {
        error("Parent sector device is NULL for new partition device.");
        return NULL;
    }

    partition_device_data *data = malloc(sizeof(partition_device_data));
    if (!data) {
        error("Memory allocation failed for partition_device_data.");
        return NULL;
    }
    data->parent = parent;
    data->start_sector = start_sector;
    data->sector_count = sector_count;

    sector_device *device = malloc(sizeof(sector_device));
    if (!device) {
        free(data);
        error("Memory allocation failed for sector_device.");
        return NULL;
    }
    device->device_data = data;
    device->get_sector_size = partition_get_sector_size;
    device->get_sector_count = partition_get_sector_count;
    device->read_sector = partition_read_sector;
    device->write_sector = partition_write_sector;
    device->dump_debug_info = partition_dump_debug_info;

    return device;
}