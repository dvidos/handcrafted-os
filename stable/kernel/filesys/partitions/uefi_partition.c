#include "../../include/bits.h"
#include "../utils/panic.h"
#include "../logger/logger.h"
#include "../klib/string.h"
#include "../memory/physmem.h"
#include "../memory/kheap.h"
#include "../../devices/block/partition_block_device.h"
#include "../../devices/devices.h"
#include "uefi_partition.h"

MODULE("UEFI_PART", LOG_LEVEL_WARN);


static error_t discover_and_register_with_buffer(block_device_t *dev, char *block_buffer) {
    error_t err = dev->ops->read_sectors(dev, 1, 1, block_buffer);
    if (err) return err;
    if (memcmp(block_buffer, "EFI PART", 8) != 0)  // Not a GPT disk
        return OK;

    uint32_t num_entries = *(uint32_t *)&block_buffer[0x50];
    uint64_t entries_lba_64 = *(uint64_t *)&block_buffer[0x48];
    if (HIGH_DWORD(entries_lba_64))
        panic("Partition entries LBA requires more than 32 bits!");
    uint32_t entries_base_lba = LOW_DWORD(entries_lba_64);
    if (entries_base_lba == 0)
        entries_base_lba = 2;

    uint32_t entry_size = *(uint32_t *)&block_buffer[0x54];
    uint32_t block_size = dev->block_size;
    int part_counter = 0;

    // To iterate through all partition entries, we need to read blocks containing them.
    // The total size of partition entries is number_of_partitions * partition_entry_size.
    // We need to read enough sectors to cover this.
    for (uint32_t i = 0; i < num_entries; i++) {
        uint32_t block_no = entries_base_lba + (i * entry_size) / block_size;
        uint32_t offset_in_block = (i * entry_size) % block_size;

        err = dev->ops->read_sectors(dev, block_no, 1, block_buffer);
        if (err) return err;
        if (mem_is_zeros(block_buffer + offset_in_block, 16))
            continue;

        uint64_t first_lba = *(uint64_t *)(block_buffer + offset_in_block + 0x20);
        uint64_t last_lba  = *(uint64_t *)(block_buffer + offset_in_block + 0x28);
        size_t   num_blocks = last_lba - first_lba + 1;
        char id[64];
        char name[128];
        block_device_t *part_dev;

        sprintfn(id, sizeof(id), "%sp%d", dev->id, part_counter);
        sprintfn(name, sizeof(name), "%s GPT partition #%d", dev->name, part_counter);
        err = create_partition_block_device(id, name, dev, first_lba, num_blocks, &part_dev);
        if (err) return err;
        register_block_device(part_dev);
        part_counter++;
    }

    return OK;
}


error_t discover_and_register_uefi_partition_block_devices(block_device_t *dev) {
    char *block_buffer = kmalloc(dev->block_size);

    error_t err = discover_and_register_with_buffer(dev, block_buffer);

    kfree(block_buffer);
    return err;
}
