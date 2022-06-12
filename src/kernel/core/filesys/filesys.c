#include <bits.h>
#include <drivers/screen.h>
#include <klog.h>
#include <klib/string.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <devices/storage_dev.h>


static char *io_page; // 4K long

struct partition {
    struct storage_dev *dev;
    char *name;
    uint32_t first_sector;
    uint32_t num_sectors;
    bool bootable;
    struct partition *next;
};

struct partition *partitions_list;

static void add_partition(struct partition *partition) {
    if (partitions_list == NULL)
        partitions_list = partition;
    else {
        struct partition *p = partitions_list;
        while (p->next != NULL)
            p = p->next;
        p->next = partition;
    }
    partition->next = NULL;
}


static bool check_gpt_partition_table(struct storage_dev *dev) {
    // we ignore LBA 0, going straight to 1.
    int err = dev->ops->read(dev, 1, 0, 1, io_page);
    klog_debug("Reading sector 1: err=%d", err);

    bool found = memcmp(io_page, "EFI PART", 8) == 0;
    if (!found) {
        klog_debug("GTP partition table not found");
        return false;
    }

    // this may mean maximum partitions, not just active, in some configurations
    uint32_t number_of_partitions = *(uint32_t*)&io_page[0x50];
    klog_debug("GPT found, has %d partitions", number_of_partitions);

    
    uint64_t partition_entries_table_address_64 = *(uint64_t*)&io_page[0x48];
    if (HIGH_DWORD(partition_entries_table_address_64))
        panic("Partition entries LBA requires more than 32 bits!");
    uint32_t partition_entries_table_address = LOW_DWORD(partition_entries_table_address_64);
    klog_debug("partition_entries_table_address 0x%x", partition_entries_table_address);
    if (partition_entries_table_address == 0)
        partition_entries_table_address = 2; // entries usually located at LBA 2

    uint32_t partition_entry_size = *(uint32_t*)&io_page[0x54];
    klog_debug("partition_entry_size %d", partition_entry_size);

    int entry_offset = 0;
    int remaining = 0;
    int sector_size = dev->ops->sector_size(dev);
    for (uint32_t i = 0; i < number_of_partitions; i++) {
        if (remaining == 0) {
            int err = dev->ops->read(dev, partition_entries_table_address, 0, 1, io_page);
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
        memcpy(type_guid, io_page + entry_offset + 0, 16);
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

        memcpy(partition_guid, io_page + entry_offset + 0x10, 16);
        starting_lba = *(uint64_t *)(io_page + entry_offset + 0x20);
        ending_lba = *(uint64_t *)(io_page + entry_offset + 0x28);
        attributes = *(uint64_t *)(io_page + entry_offset + 0x30);
        if (HIGH_DWORD(starting_lba) != 0 || HIGH_DWORD(ending_lba) != 0)
            panic("Partition sectors contain addresses > 32 bits long");
        
        klog_debug("Part %d, type=%02x%02x%02x%02x start=0x%08x:%08x, end=0x%08x:%08x, attr=0x%08x:%08x",
            i,
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
            i,
            partition_guid[0], partition_guid[1], partition_guid[2], partition_guid[3], 
            partition_guid[4], partition_guid[5], partition_guid[6], partition_guid[7], 
            partition_guid[8], partition_guid[9], partition_guid[10], partition_guid[11], 
            partition_guid[12], partition_guid[13], partition_guid[14], partition_guid[15]);
        struct partition *part = kmalloc(sizeof(struct partition));
        part->dev = dev;
        part->name = name;
        part->first_sector = LOW_DWORD(starting_lba);
        part->num_sectors = ending_lba - starting_lba;
        part->bootable = (attributes & 0x02);
        add_partition(part);

        // move to the next
        entry_offset += partition_entry_size;
        remaining -= partition_entry_size;
    }

    return found;
}

static bool check_legacy_partition_table(struct storage_dev *dev) {
    int err = dev->ops->read(dev, 0, 0, 1, io_page);
    // klog_debug("Reading sector 0: err=%d", err);

    klog_debug("Looking for legacy partition");
    klog_hex16_info(io_page + 0x1BE, 64, 0x1BE);


    // partition entries at 1BE, 1CE, 1DE, 1EE
    for (int offset = 0x1BE; offset < 0x1FE; offset += 0x10) {
        bool all_zeros = true;
        for (int i = 0; i < 16; i++) {
            if (io_page[offset + i] != 0) {
                all_zeros = false;
                break;
            }
        }
        if (all_zeros)
            continue;

        klog_debug("Found legacy partition table entry at 0x%x !!!", offset);
        klog_debug("attr 0x%x, type 0x%x, first sector %d, sectors %d", 
            (uint8_t)io_page[offset + 0x0],
            (uint8_t)io_page[offset + 0x4],
            (uint32_t)io_page[offset + 0x8],
            (uint32_t)io_page[offset + 0xC]
        );
    }

    return false;
}

static void check_storage_device(struct storage_dev *dev) {
    // see if there is a partition table
    // for each partition, see if there is a file system
    // find a way to mount each disk in a... namespace.

    // klog_debug("Looking for MBR on disk %d", dev->dev_no);
    // int sectors = 8;
    // dev->ops.read(dev, 0, 0, sectors, io_page);
    // klog_hex16_info(io_page, sectors * 512, 0);    
    // check_legacy_partition_table(io_page);
    // check_uefi_partition_table(io_page);

    // first we check for GPT, and if nothing if found, 
    bool found = check_gpt_partition_table(dev);
    if (!found) {
        // check legacy only if GPT not found
        found = check_legacy_partition_table(dev);
    }
}

void init_filesys() {
    io_page = allocate_physical_page();

    // see if we have any disks
    struct storage_dev *dev = storage_mgr_get_devices_list();
    while (dev != NULL) {
        klog_debug("fs: checking storage device \"%s\"", dev->name);
        check_storage_device(dev);
        dev = dev->next;
    }

    free_physical_page(io_page);
}
