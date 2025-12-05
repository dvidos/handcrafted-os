#pragma once

#include <stdint.h>

typedef struct sector_device sector_device;

struct sector_device {
    uint32_t (*get_sector_size)(sector_device *device);  // typically 512 bytes
    uint32_t (*get_sector_count)(sector_device *device); // how many sectors

    int (*read_sector)(sector_device *device, uint32_t sector_no, uint8_t *buffer);
    int (*write_sector)(sector_device *device, uint32_t sector_no, uint8_t *buffer);

    void (*dump_debug_info)(sector_device *device, const char *title);
    void *device_data;
};

sector_device *new_mem_based_sector_device(int sector_size, uint32_t sector_count);