#include <bits.h>
#include <drivers/screen.h>
#include <klog.h>
#include <klib/string.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <devices/storage_dev.h>
#include <filesys/partition.h>

static struct partition *partitions_list = NULL;

struct partition *get_partitions_list() {
    return partitions_list;
}

static void add_partition(struct partition *partition) {
    if (partitions_list == NULL) {
        partitions_list = partition;
    } else {
        struct partition *p = partitions_list;
        while (p->next != NULL)
            p = p->next;
        p->next = partition;
    }
    partition->next = NULL;
}

static bool check_gpt_partition_table(struct storage_dev *dev, char *buffer) {
    // we ignore LBA 0, going straight to 1.
    int err = dev->ops->read(dev, 1, 0, 1, buffer);
    klog_debug("Reading sector 1: err=%d", err);
    if (err)
        return false;

    bool found = memcmp(buffer, "EFI PART", 8) == 0;
    if (!found) {
        klog_debug("GTP partition table not found");
        return false;
    }

    // this may mean maximum partitions, not just active, in some configurations
    uint32_t number_of_partitions = *(uint32_t*)&buffer[0x50];
    klog_debug("GPT found, has %d partitions", number_of_partitions);
    
    uint64_t partition_entries_table_address_64 = *(uint64_t*)&buffer[0x48];
    if (HIGH_DWORD(partition_entries_table_address_64))
        panic("Partition entries LBA requires more than 32 bits!");
    uint32_t partition_entries_table_address = LOW_DWORD(partition_entries_table_address_64);
    klog_debug("partition_entries_table_address 0x%x", partition_entries_table_address);
    if (partition_entries_table_address == 0)
        partition_entries_table_address = 2; // entries usually located at LBA 2

    uint32_t partition_entry_size = *(uint32_t*)&buffer[0x54];
    klog_debug("partition_entry_size %d", partition_entry_size);

    int entry_offset = 0;
    int remaining = 0;
    int sector_size = dev->ops->sector_size(dev);
    for (uint32_t part_no = 0; part_no < number_of_partitions; part_no++) {
        if (remaining == 0) {
            int err = dev->ops->read(dev, partition_entries_table_address, 0, 1, buffer);
            // klog_debug("Reading sector %d: err=%d", partition_entries_table_address, err);
            // klog_hex16_info(io_page, sector_size, 0);
            partition_entries_table_address++;
            remaining += sector_size;
            entry_offset = 0;
        }

        uint8_t type_guid[16];
        uint8_t partition_guid[16];
        uint64_t starting_lba;
        uint64_t ending_lba;
        uint64_t attributes;

        // an all zeros type GUID means partition entry is unused
        memcpy(type_guid, buffer + entry_offset + 0, 16);
        bool all_zeros = true;
        for (int i = 0; i < 16; i++) {
            if (type_guid[i] != 0) {
                all_zeros = false;
                break;
            }
        }
        if (all_zeros) {
            entry_offset += partition_entry_size;
            remaining -= partition_entry_size;
            continue;
        }

        memcpy(partition_guid, buffer + entry_offset + 0x10, 16);
        starting_lba = *(uint64_t *)(buffer + entry_offset + 0x20);
        ending_lba = *(uint64_t *)(buffer + entry_offset + 0x28);
        attributes = *(uint64_t *)(buffer + entry_offset + 0x30);
        if (HIGH_DWORD(starting_lba) != 0 || HIGH_DWORD(ending_lba) != 0)
            panic("Partition sectors contain addresses > 32 bits long");
        
        klog_debug("Part %d, type=%02x%02x%02x%02x start=0x%08x:%08x, end=0x%08x:%08x, attr=0x%08x:%08x",
            part_no,
            type_guid[0], type_guid[1], type_guid[2], type_guid[3], 
            HIGH_DWORD(starting_lba), LOW_DWORD(starting_lba),
            HIGH_DWORD(ending_lba), LOW_DWORD(ending_lba),
            HIGH_DWORD(attributes), LOW_DWORD(attributes)
        );

        if (attributes & 0x01) {
            // this is required by firmware and should not be touched
        }
        if (attributes & 0x02) {
            // contains files to boot an operating system
        }

        char *name = kmalloc(64);
        sprintfn(name, 64, "Partition %d (%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
            part_no,
            partition_guid[0], partition_guid[1], partition_guid[2], partition_guid[3], 
            partition_guid[4], partition_guid[5], partition_guid[6], partition_guid[7], 
            partition_guid[8], partition_guid[9], partition_guid[10], partition_guid[11], 
            partition_guid[12], partition_guid[13], partition_guid[14], partition_guid[15]);
        struct partition *part = kmalloc(sizeof(struct partition));
        part->name = name;
        part->part_no = part_no + 1;
        part->dev = dev;
        part->first_sector = LOW_DWORD(starting_lba);
        part->num_sectors = ending_lba - starting_lba;
        part->bootable = (attributes & 0x02);
        add_partition(part);

        // move to the next
        entry_offset += partition_entry_size;
        remaining -= partition_entry_size;
    }

    return true;
}


static bool check_legacy_partition_table(struct storage_dev *dev, uint32_t starting_sector, uint8_t ext_part_num, char *buffer) {
    int err = dev->ops->read(dev, starting_sector, 0, 1, buffer);
    klog_debug("Reading sector %d: err=%d", starting_sector, err);
    if (err)
        return false;
    
    // using this to mark the discovery of an extended partition as well.
    uint32_t extended_partition_offset = 0;

    klog_debug("Looking for legacy partition, at sector %d", starting_sector);
    // klog_hex16_info(buffer + 0x1BE, 64, 0x1BE);

    // partition entries at 1BE, 1CE, 1DE, 1EE
    bool found_something = false;
    for (int entry_no = 0; entry_no < 4; entry_no++) {
        int offset = 0x1BE + entry_no * 0x10;
        uint8_t  boot_indicator = *(uint8_t  *)(buffer + offset + 0x0);
        uint8_t  start_head     = *(uint8_t  *)(buffer + offset + 0x1);
        uint8_t  start_sector   = *(uint8_t  *)(buffer + offset + 0x2);
        uint16_t start_cylinder = *(uint8_t  *)(buffer + offset + 0x3);
        uint8_t  system_id      = *(uint8_t  *)(buffer + offset + 0x4);
        uint8_t  end_head       = *(uint8_t  *)(buffer + offset + 0x5);
        uint8_t  end_sector     = *(uint8_t  *)(buffer + offset + 0x6);
        uint16_t end_cylinder   = *(uint8_t  *)(buffer + offset + 0x7);
        uint32_t sector_offset  = *(uint32_t *)(buffer + offset + 0x8);
        uint32_t num_sectors    = *(uint32_t *)(buffer + offset + 0xC);
        
        // two bits on upper byte of cylinders are stored in bits 6 &7 of sector
        start_cylinder = ((start_sector >> 6) << 8) | start_cylinder;
        start_sector = start_sector & 0x3F;
        end_cylinder = ((end_sector >> 6) << 8) | end_cylinder;
        end_sector = end_sector & 0x3F;

        klog_debug("Entry %d data: boot=0x%02x, id=%02x, CHS start %d,%d,%d end %d,%d,%d, lba=%d, sectors=%d",
            entry_no,
            boot_indicator,
            system_id,
            start_cylinder,
            start_head,
            start_sector,
            end_cylinder,
            end_head,
            end_sector,
            sector_offset,
            num_sectors
        );

        if (system_id == 0x00)
            continue;
        
        if (system_id == 0x05 || system_id == 0x0F) {
            extended_partition_offset = sector_offset;
            continue;
        }

        struct partition *part = kmalloc(sizeof(struct partition));
        part->name = kmalloc(64);
        if (starting_sector == 0) {
            part->part_no = entry_no + 1;
            sprintfn(part->name, 64, "Primary partition %d", part->part_no);
        } else {
            part->part_no = ext_part_num++;
            sprintfn(part->name, 64, "Logical partition %d", part->part_no);
        }
        part->bootable = IS_BIT(boot_indicator, 7);
        part->first_sector = starting_sector + sector_offset;
        part->num_sectors = num_sectors;
        part->legacy_type = system_id;
        part->dev = dev;
        found_something = true;
        add_partition(part);
    }

    // now that we're done parsing our shared buffer, we can recurse
    if (extended_partition_offset) {
        if (check_legacy_partition_table(dev, starting_sector + extended_partition_offset, ext_part_num, buffer))
            found_something = true;
    }

    return found_something;
}

static void check_storage_device(struct storage_dev *dev, char *buffer) {
    // first we check for GPT, and if nothing if found, 
    if (check_gpt_partition_table(dev, buffer))
        return;
    check_legacy_partition_table(dev, 0, 5, buffer);

    // there may be a case we have a filesystem without partitions,
    // but it's an exception to the rule. We won't support this for now.
}

void discover_storage_dev_partitions(struct storage_dev *devices_list) {
    struct storage_dev *dev;

    // see if we have any disks
    dev = devices_list;
    char *buffer = allocate_physical_page();
    while (dev != NULL) {
        klog_debug("fs: checking storage device \"%s\"", dev->name);
        check_storage_device(dev, buffer);
        dev = dev->next;
    }
    free_physical_page(buffer);

    // print disks available
    dev = devices_list;
    klog_info("Disks found:");
    while (dev != NULL) {
        klog_info("- dev #%d: %s", dev->dev_no, dev->name);
        dev = dev->next;
    }

    // now print them
    struct partition *part = partitions_list;
    klog_info("Partitions found:");
    while (part != NULL) {
        klog_info("- dev #%d, p #%d: %s (%d sectors, start at %d)", 
            part->dev->dev_no, part->part_no, part->name, part->num_sectors, part->first_sector);
        part = part->next;
    }
}
