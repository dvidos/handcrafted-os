#include <bits.h>
#include "../utils/panic.h"
#include "../utils/logger.h"
#include "../klib/string.h"
#include "../memory/physmem2.h"
#include "../memory/kheap.h"
#include "../../devices/block/partition_block_device.h"
#include "../../devices/devices.h"
#include "legacy_partition.h"


MODULE("LGC_PART", LOG_LEVEL_WARN);

typedef struct {
    uint8_t  boot_indicator;
    uint8_t  system_id;
    uint32_t lba_start;
    uint32_t num_sectors;
    uint8_t  start_head;
    uint8_t  start_sector;
    uint16_t start_cylinder;
    uint8_t  end_head;
    uint8_t  end_sector;
    uint16_t end_cylinder;
} mbr_partition_entry_t;

static mbr_partition_entry_t read_mbr_partition_entry(const char *block_buffer, int index) {
    mbr_partition_entry_t entry;
    int offset = 0x1BE + index * 0x10;

    entry.boot_indicator = *(uint8_t  *)(block_buffer + offset + 0x0);
    entry.start_head     = *(uint8_t  *)(block_buffer + offset + 0x1);
    entry.start_sector   = *(uint8_t  *)(block_buffer + offset + 0x2);
    entry.start_cylinder = *(uint8_t  *)(block_buffer + offset + 0x3);
    entry.system_id      = *(uint8_t  *)(block_buffer + offset + 0x4);
    entry.end_head       = *(uint8_t  *)(block_buffer + offset + 0x5);
    entry.end_sector     = *(uint8_t  *)(block_buffer + offset + 0x6);
    entry.end_cylinder   = *(uint8_t  *)(block_buffer + offset + 0x7);
    entry.lba_start      = *(uint32_t *)(block_buffer + offset + 0x8);
    entry.num_sectors    = *(uint32_t *)(block_buffer + offset + 0xC);

    // CHS conversion (as seen in older_code__check_legacy_partition_table)
    entry.start_cylinder = ((entry.start_sector >> 6) << 8) | entry.start_cylinder;
    entry.start_sector = entry.start_sector & 0x3F;
    entry.end_cylinder = ((entry.end_sector >> 6) << 8) | entry.end_cylinder;
    entry.end_sector = entry.end_sector & 0x3F;

    return entry;
}

static error_t create_and_register_partition_dev(block_device_t *dev, int *part_count, uint64_t first_block, size_t num_blocks, bool primary) {
    char id[64];
    char name[128];
    sprintfn(id, sizeof(id), "%sp%d", dev->id, *part_count);
    sprintfn(name, sizeof(name), "%s %s partition #%d", dev->name, primary ? "primary" : "logical", *part_count);

    block_device_t *part_dev;
    error_t err = create_partition_block_device(id, name, dev, first_block, num_blocks, &part_dev);
    if (err) return err;

    register_block_device(part_dev);
    (*part_count)++;
    return OK;
}

static error_t discover_and_register_extended_partitions(
    block_device_t *dev,
    uint32_t current_ebr_lba,
    uint32_t extended_base_lba,
    int *part_count,
    char *block_buffer
) {
    error_t err = dev->ops->read(dev, current_ebr_lba, 1, block_buffer);
    if (err) return err;

    // Entry 0: logical partition
    mbr_partition_entry_t part = read_mbr_partition_entry(block_buffer, 0);
    if (part.system_id != 0x00) {
        uint32_t part_lba_start = current_ebr_lba + part.lba_start;
        err = create_and_register_partition_dev(dev, part_count, part_lba_start, part.num_sectors, false);
        if (err) return err;
    }

    // Entry 1: pointer to next EBR
    mbr_partition_entry_t next = read_mbr_partition_entry(block_buffer, 1);
    if (next.system_id == 0x05 || next.system_id == 0x0F) {
        uint32_t next_ebr_lba = extended_base_lba + next.lba_start;
        return discover_and_register_extended_partitions(dev, next_ebr_lba, extended_base_lba, part_count, block_buffer);
    }

    return OK;
}

error_t discover_and_register_partitions_with_buffer(block_device_t *dev, char *block_buffer) {
    int part_count = 0;

    // Read primary MBR
    error_t err = dev->ops->read(dev, 0, 1, block_buffer);
    if (err) return err;

    for (int i = 0; i < 4; i++) {
        mbr_partition_entry_t entry = read_mbr_partition_entry(block_buffer, i);
        if (entry.system_id == 0x00) continue;

        if (entry.system_id == 0x05 || entry.system_id == 0x0F) {
            uint32_t extended_base_lba = entry.lba_start;
            uint32_t first_ebr_lba = extended_base_lba;
            err = discover_and_register_extended_partitions(dev, first_ebr_lba, extended_base_lba, &part_count, block_buffer);
            if (err) return err;

        } else {
            err = create_and_register_partition_dev(dev, &part_count, entry.lba_start, entry.num_sectors, true);
            if (err) return err;
        }
    }

    return OK;
}

error_t discover_and_register_legacy_partition_block_devices(block_device_t *dev) {
    char *block_buffer = kmalloc(dev->block_size);

    error_t err = discover_and_register_partitions_with_buffer(dev, block_buffer);

    kfree(block_buffer);
    return err;
}
