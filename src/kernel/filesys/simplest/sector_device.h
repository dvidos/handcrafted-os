#pragma once

typedef struct sector_device      sector_device;


typedef struct sector_device {
    unsigned (*get_sector_size)(sector_device *device);  // typically 512 bytes
    unsigned (*get_sector_count)(sector_device *device); // how many sectors

    int (*read_sector)(sector_device *device, int sector_no, char *buffer);
    int (*write_sector)(sector_device *device, int sector_no, char *buffer);

    void *device_data;
} sector_device;
