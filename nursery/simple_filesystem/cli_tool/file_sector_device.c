#include "file_sector_device.h"
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_SECTOR_SIZE 512

typedef struct {
    FILE *file;
    uint32_t sector_size;
    uint32_t sector_count;
} file_device_data;

static uint32_t file_get_sector_size(sector_device *device) {
    file_device_data *data = (file_device_data *)device->device_data;
    return data->sector_size;
}

static uint32_t file_get_sector_count(sector_device *device) {
    file_device_data *data = (file_device_data *)device->device_data;
    return data->sector_count;
}

static int file_read_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    file_device_data *data = (file_device_data *)device->device_data;
    if (fseek(data->file, sector_no * data->sector_size, SEEK_SET) != 0) {
        return -1; // Error
    }
    if (fread(buffer, data->sector_size, 1, data->file) != 1) {
        return -1; // Error
    }
    return 0; // Success
}

static int file_write_sector(sector_device *device, uint32_t sector_no, uint8_t *buffer) {
    file_device_data *data = (file_device_data *)device->device_data;
    if (fseek(data->file, sector_no * data->sector_size, SEEK_SET) != 0) {
        return -1; // Error
    }
    if (fwrite(buffer, data->sector_size, 1, data->file) != 1) {
        return -1; // Error
    }
    return 0; // Success
}

static void file_dump_debug_info(sector_device *device, const char *title) {
    // Implementation can be added if needed
}

sector_device *new_file_sector_device(const char *filename) {
    FILE *file = fopen(filename, "r+b");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    file_device_data *data = malloc(sizeof(file_device_data));
    data->file = file;
    data->sector_size = DEFAULT_SECTOR_SIZE;
    data->sector_count = file_size / DEFAULT_SECTOR_SIZE;

    sector_device *device = malloc(sizeof(sector_device));
    device->device_data = data;
    device->get_sector_size = file_get_sector_size;
    device->get_sector_count = file_get_sector_count;
    device->read_sector = file_read_sector;
    device->write_sector = file_write_sector;
    device->dump_debug_info = file_dump_debug_info;

    return device;
}
