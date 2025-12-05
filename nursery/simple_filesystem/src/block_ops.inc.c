#include "internal.h"


static int read_block(sector_device *dev, uint32_t bytes_per_sector, uint32_t sectors_per_block, uint32_t block_id, void *buffer) {
    if (dev == NULL)
        return ERR_NOT_SUPPORTED;

    int sector_no = block_id * sectors_per_block;
    while (sectors_per_block-- > 0) {
        int err = dev->read_sector(dev, sector_no, buffer);
        if (err != OK) return err;
        sector_no += 1;
        buffer += bytes_per_sector;
    }

    return OK;
}

static int write_block(sector_device *dev, uint32_t bytes_per_sector, uint32_t sectors_per_block, uint32_t block_id, void *buffer) {
    if (dev == NULL)
        return ERR_NOT_SUPPORTED;

    int sector_no = block_id * sectors_per_block;
    while (sectors_per_block-- > 0) {
        int err = dev->write_sector(dev, sector_no, buffer);
        if (err != OK) return err;
        sector_no += 1;
        buffer += bytes_per_sector;
    }

    return OK;
}



