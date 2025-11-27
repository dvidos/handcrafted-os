#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
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

static void dump_debug_info(sector_device *device, const char *title) {
    sector_device_data *sdd = (sector_device_data *)device->device_data;

    printf("%s\n", title);

    /*
        00011700  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
        *
        00011730  00 00 00 00 00 00 00 00  1b 00 00 00 01 00 00 00  |................|
        00011e70  00 00 00 00 00 00 00 00  35 01 00 00 01 00 00 00  |........5.......|
        00011e80  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
        00011e90  36 cc 00 00 00 00 00 00  c8 06 00 00 00 00 00 00  |6...............|  
    */

    char prev_dump_info[128];
    char dump_info[128];
    int rows_count = (sdd->sector_count * sdd->sector_size) / 16;
    uint32_t offset = 0;
    int have_asterisk;

    for (int row = 0; row < rows_count; row++) {

        unsigned char *p = sdd->data + offset;
        sprintf(dump_info, "%02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  |%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c|",
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15],
            p[ 0] == 0 ? '.' : (isprint(p[ 0]) ? p[ 0] : '#'),
            p[ 1] == 0 ? '.' : (isprint(p[ 1]) ? p[ 1] : '#'),
            p[ 2] == 0 ? '.' : (isprint(p[ 2]) ? p[ 2] : '#'),
            p[ 3] == 0 ? '.' : (isprint(p[ 3]) ? p[ 3] : '#'),
            p[ 4] == 0 ? '.' : (isprint(p[ 4]) ? p[ 4] : '#'),
            p[ 5] == 0 ? '.' : (isprint(p[ 5]) ? p[ 5] : '#'),
            p[ 6] == 0 ? '.' : (isprint(p[ 6]) ? p[ 6] : '#'),
            p[ 7] == 0 ? '.' : (isprint(p[ 7]) ? p[ 7] : '#'),
            p[ 8] == 0 ? '.' : (isprint(p[ 8]) ? p[ 8] : '#'),
            p[ 9] == 0 ? '.' : (isprint(p[ 9]) ? p[ 9] : '#'),
            p[10] == 0 ? '.' : (isprint(p[10]) ? p[10] : '#'),
            p[11] == 0 ? '.' : (isprint(p[11]) ? p[11] : '#'),
            p[12] == 0 ? '.' : (isprint(p[12]) ? p[12] : '#'),
            p[13] == 0 ? '.' : (isprint(p[13]) ? p[13] : '#'),
            p[14] == 0 ? '.' : (isprint(p[14]) ? p[14] : '#'),
            p[15] == 0 ? '.' : (isprint(p[15]) ? p[15] : '#')
        );

        // avoid repetitions
        if (row > 0 && row < rows_count - 1 && strcmp(dump_info, prev_dump_info) == 0) {
            if (!have_asterisk)
                printf("                *\n");
            have_asterisk = 1;
            offset += 16;
            continue;
        }

        printf("%4x  %08x  %s\n", offset / sdd->sector_size, offset, dump_info);
        strcpy(prev_dump_info, dump_info);
        have_asterisk = 0;
        offset += 16;
    }
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
    d->dump_debug_info = dump_debug_info;
    return d;
}
