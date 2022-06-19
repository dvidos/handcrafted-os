#include <bits.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <klog.h>
#include <klib/string.h>

#define min(a, b)  ((a) < (b) ? (a) : (b))

// mainly based on https://www.pjrc.com/tech/8051/ide/fat32.html
// and https://wiki.osdev.org/FAT

typedef struct fat_boot_sector_extension_12_16 // applies to FAT12 as well
{
	//extended fat12 and fat16 stuff
	unsigned char		bios_drive_num;
	unsigned char		reserved1;
	unsigned char		boot_signature;
	unsigned int		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
}__attribute__((packed)) fat_boot_sector_extension_12_16_t;

typedef struct fat_boot_sector_extension_32
{
	unsigned int		sectors_per_fat_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_dir_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
}__attribute__((packed)) fat_boot_sector_extension_32_t;

typedef struct fat_boot_sector
{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short 	    bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		number_of_fats;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16bits;
	unsigned char		media_type;
	unsigned short		sectors_per_fat_16; // for 12/16 only
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32bits;

	// use the proper entry, once the driver actually knows what type of FAT this is.
    union {
        fat_boot_sector_extension_32_t fat_32;
        fat_boot_sector_extension_12_16_t fat_12_16;
    } types;

    char padding[420];
    uint16_t bootable_sector_signature;

}__attribute__((packed)) fat_boot_sector_t;


enum FAT_TYPE { FAT32, FAT16, FAT12 };

struct fat_info {
    struct partition *partition;
    fat_boot_sector_t *boot_sector;
    enum FAT_TYPE fat_type;

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t sectors_per_fat;

    // absolute addressing, relative to disk
    uint32_t fat_starting_lba;
    uint32_t root_dir_starting_lba;
    uint32_t data_clusters_starting_lba;
    uint32_t root_dir_sectors_size; // for FAT12/16 only

    // allocated bufers for reading sectors and clusters
    uint8_t *sector_buffer;
    uint8_t *fat_buffer;
    uint8_t *cluster_buffer;
};

struct dir_entry {
    char short_name[12+1]; // 8 + dot + 3 + zero term
    char long_name_ucs2[512 + 2]; // UCS-2 format

    union {
        uint8_t value;
        struct {
            uint8_t read_only: 1;
            uint8_t hidden: 1;
            uint8_t system: 1;
            uint8_t volume_label: 1;
            uint8_t directory: 1;
            uint8_t archive: 1;
        }__attribute__((packed)) flags;
    } attributes;

    uint32_t first_cluster;
    uint32_t file_size;

    uint16_t created_year;
    uint8_t created_month;
    uint8_t created_day;
    uint8_t created_hour;
    uint8_t created_min;
    uint8_t created_sec;

    uint16_t modified_year;
    uint8_t modified_month;
    uint8_t modified_day;
    uint8_t modified_hour;
    uint8_t modified_min;
    uint8_t modified_sec;
};

#define ATTR_READONLY   0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOL_LABEL  0x08
#define ATTR_SUB_DIR    0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F  // hack!

#define DIR_NAME_DELETED      0xE5
#define DIR_NAME_END_OF_LIST  0x00
 

static uint32_t get_next_cluster_no(struct fat_info *info, uint32_t current_cluster_no) {

    if (info->fat_type == FAT12) {
        // multiply by 1.5 is 1 + (1/2), rounding down
        uint32_t offset = current_cluster_no + (current_cluster_no / 2);
        uint16_t word = *(uint16_t *)&info->fat_buffer[offset];

        // see if we rounded down
        if (current_cluster_no & 0x0001) {
            word >>= 4; // use three higher nibbles
        } else {
            word &= 0xFFF; // use three lower nibbles
        }
    } else if (info->fat_type == FAT16) {
        uint32_t offset = current_cluster_no * 2; // two bytes for 16 bits
        uint32_t next_cluster_no = *(uint16_t *)&info->fat_buffer[offset];
        return next_cluster_no;
    } else if (info->fat_type == FAT32) {
        uint32_t offset = current_cluster_no * 4; // four bytes for 32 bits
        uint32_t next_cluster_no = *(uint32_t *)&info->fat_buffer[offset];
        return next_cluster_no;
    }

    // else return none
    return 0xFFFFFFFF;
}

static int read_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer) {
    int err = info->partition->dev->ops->read(
        info->partition->dev,
        info->data_clusters_starting_lba + ((cluster_no - 2) * info->sectors_per_cluster),
        0,
        info->sectors_per_cluster,
        buffer
    );
    klog_trace("read_cluster(info, %d, 0x%p) --> %d", cluster_no, buffer, err);
    return err;
}

static int write_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer) {
    return -1;
}

static int read_fat(struct fat_info *info) {
    return info->partition->dev->ops->read(
        info->partition->dev,
        info->fat_starting_lba,
        0,
        info->sectors_per_fat,
        info->fat_buffer
    );
}

static int write_fat(struct fat_info *info) {
    // we should write twice
    return -1;
}

static void debug_fat_info(struct fat_info *info) {
    klog_debug("FAT info");
    klog_debug("  type                   FAT%d", info->fat_type == FAT32 ? 32 : (info->fat_type == FAT16 ? 16 : 12));
    klog_debug("  bytes per sector       %d", info->bytes_per_sector);
    klog_debug("  sectors per cluster    %d", info->sectors_per_cluster);
    klog_debug("  cluster size in bytes  %d", info->sectors_per_cluster * info->bytes_per_sector);
    klog_debug("  sectors per fat        %d", info->sectors_per_fat);
    klog_debug("  fat size in bytes      %d", info->sectors_per_fat * info->bytes_per_sector);
    klog_debug("  number of fats         %d", info->boot_sector->number_of_fats);
    klog_debug("  ---");
    klog_debug("  reserved sector count  %d", info->boot_sector->reserved_sector_count);
    klog_debug("  sectors per fat-16     %d", info->boot_sector->sectors_per_fat_16);
    klog_debug("  sectors per fat-32     %d", info->boot_sector->types.fat_32.sectors_per_fat_32);
    klog_debug("  total sectors 16 bits  %d", info->boot_sector->total_sectors_16bits);
    klog_debug("  total sectors 32 bits  %d", info->boot_sector->total_sectors_32bits);
    klog_debug("  ---");
    klog_debug("  partition start lba         %d", info->partition->first_sector);
    klog_debug("  fat starting lba            %d", info->fat_starting_lba);
    klog_debug("  data clusters starting lba  %d", info->data_clusters_starting_lba);
    klog_debug("  root dir starting lba       %d", info->root_dir_starting_lba);
    klog_debug("  root dir sectors size       %d", info->root_dir_sectors_size);
}

static void debug_dir_entry(bool title, struct dir_entry *entry) {
    if (title) {
        //         |  ABCDEFGH.ABC 0x12 RHADSV 1234-12-12 12:12:12 1234-12-12 12:12:12 123456789 123456789 Xxxxxxxx
        klog_debug("  File name    Attr Flags  Cr.Date    Cr.Time  Mod.Date   Mod.Time      Size   Cluster Long file name");
    } else {
        char cstr[256+1];
        ucs2str_to_cstr(entry->long_name_ucs2, cstr);
        klog_debug(
            "  %-12s 0x%02x %c%c%c%c%c%c %04d-%02d-%02d %02d:%02d:%02d %04d-%02d-%02d %02d:%02d:%02d %9d %9d %s",
            entry->short_name,
            entry->attributes.value,
            entry->attributes.flags.read_only ? 'R' : '-',
            entry->attributes.flags.hidden ? 'H' : '-',
            entry->attributes.flags.archive ? 'A' : '-',
            entry->attributes.flags.directory ? 'D' : '-',
            entry->attributes.flags.volume_label ? 'V' : '-',
            entry->attributes.flags.system ? 'S' : '-',

            entry->created_year,
            entry->created_month,
            entry->created_day,
            entry->created_hour,
            entry->created_min,
            entry->created_sec,

            entry->modified_year,
            entry->modified_month,
            entry->modified_day,
            entry->modified_hour,
            entry->modified_min,
            entry->modified_sec,

            entry->file_size,
            entry->first_cluster,
            cstr
        );
    }
}

// reads a directory entry and advances offset. returns false when no nore entries available.
static bool get_dir_entry(uint8_t *buffer, int buffer_len, int *offset, struct dir_entry *entry) {
    memset(entry, 0, sizeof(struct dir_entry));
    while (true) {
        // if we went past the buffer, no valid entry must be found
        if (*offset >= buffer_len)
            return false; // no more

        // see what this entry is about
        uint8_t attributes = buffer[*offset + 0x0B];
        uint8_t first_name_byte = buffer[*offset];

        // if long name, collect it for now
        if (attributes == ATTR_LONG_NAME) {
            uint8_t seq_no = buffer[*offset] & 0x3F; // starts with 1
            uint8_t is_last = buffer[*offset] & 0x40;
            // collect them into the long_entry field
            // each dir entry in the buffer can hold up to 13 UCS-2 characters
            int long_offset = (seq_no - 1) * (13 * 2);
            memcpy(entry->long_name_ucs2 + long_offset +  0, buffer + *offset + 0x01, 10);
            memcpy(entry->long_name_ucs2 + long_offset + 10, buffer + *offset + 0x0E, 12);
            memcpy(entry->long_name_ucs2 + long_offset + 22, buffer + *offset + 0x1C,  4);
            (*offset) += 32;
            continue;
        }

        // if zeros, we finished reading
        if (first_name_byte == DIR_NAME_END_OF_LIST) {
            *offset += 32;
            return false;
        }
        
        // if deleted entry, skip this one
        if (first_name_byte == DIR_NAME_DELETED) {
            *offset += 32;
            continue;
        }
        
        // so we should have a normal, short entry, parse it.
        entry->attributes.value = attributes;
        uint16_t cluster_hi = *(uint16_t*)&buffer[*offset + 0x14];
        uint16_t cluster_low = *(uint16_t*)&buffer[*offset + 0x1a];
        entry->first_cluster = ((uint32_t)cluster_hi) << 16 | (uint32_t)cluster_low;
        entry->file_size = *(uint32_t *)&buffer[*offset + 0x1c];

        uint16_t timestamp = *(uint16_t *)&buffer[*offset + 0x0E];
        entry->created_hour = BIT_RANGE(timestamp, 15, 11);
        entry->created_min  = BIT_RANGE(timestamp, 10,  5);
        entry->created_sec  = BIT_RANGE(timestamp,  4,  0) * 2; // stores secs/2

        timestamp = *(uint16_t *)&buffer[*offset + 0x10];
        entry->created_year = 1980 + BIT_RANGE(timestamp, 15, 9);
        entry->created_month = BIT_RANGE(timestamp, 8, 5);
        entry->created_day = BIT_RANGE(timestamp, 4, 0);

        timestamp = *(uint16_t *)&buffer[*offset + 0x16];
        entry->modified_hour = BIT_RANGE(timestamp, 15, 11);
        entry->modified_min  = BIT_RANGE(timestamp, 10,  5);
        entry->modified_sec  = BIT_RANGE(timestamp,  4,  0) * 2; // stores secs/2

        timestamp = *(uint16_t *)&buffer[*offset + 0x18];
        entry->modified_year = 1980 + BIT_RANGE(timestamp, 15, 9);
        entry->modified_month = BIT_RANGE(timestamp, 8, 5);
        entry->modified_day = BIT_RANGE(timestamp, 4, 0);

        // parse short name
        memset(entry->short_name, 0, sizeof(entry->short_name));
        for (int i = 0; i < 8; i++)
            if (buffer[*offset + i] != ' ')
                entry->short_name[i] = buffer[*offset + i];
        if (buffer[*offset + 8] != ' ')
            entry->short_name[strlen(entry->short_name)] = '.';
        for (int i = 8; i < 11; i++)
            if (buffer[*offset + i] != ' ')
                entry->short_name[strlen(entry->short_name)] = buffer[*offset + i];

        // if we gotten here, we just got an entry
        (*offset) += 32;
        return true;
    }
}

static int probe(struct partition *partition);
static struct file_system_driver vfs_driver = {
    .name = "FAT",
    .probe = probe
};
void fat_register_vfs_driver() {
    vfs_register_file_system_driver(&vfs_driver);
}

#define ERR_NO_FAT   -1

static int probe(struct partition *partition) {
    klog_trace("FAT probing partition #%d (legacy type 0x%02x)", partition->part_no, partition->legacy_type);

    if (sizeof(fat_boot_sector_t) != 512) {
        klog_error("Boot sector struct would not align with reading of a sector");
        return ERR_NO_FAT;
    }
    
    uint8_t *page = allocate_physical_page();

    // let's check the first sector of the partition.
    fat_boot_sector_t *boot_sector = kmalloc(sizeof(fat_boot_sector_t));
    memset(boot_sector, 0, sizeof(fat_boot_sector_t));
    int err = partition->dev->ops->read(
        partition->dev, 
        partition->first_sector, 0,
        1,
        (char *)boot_sector
    );
    if (err) {
        klog_debug("Err %d reading first sector of partition", err);
        kfree(boot_sector);
        return err;
    }
    
    if (boot_sector->bootjmp[0] != 0xEB ||
        boot_sector->bytes_per_sector != 512 ||
        boot_sector->number_of_fats != 2 ||
        (boot_sector->total_sectors_16bits == 0 && boot_sector->total_sectors_32bits == 0) ||
        (
            boot_sector->types.fat_12_16.boot_signature != 0x28 &&
            boot_sector->types.fat_12_16.boot_signature != 0x29 && 
            boot_sector->types.fat_32.boot_signature != 0x28 &&
            boot_sector->types.fat_32.boot_signature != 0x29
        ) ||
        boot_sector->bootable_sector_signature != 0xAA55
    ) {
        klog_debug("Does not look like a FAT MBR");
        kfree(boot_sector);
        return ERR_NO_FAT;
    }

    struct fat_info *info = kmalloc(sizeof(struct fat_info));
    memset(info, 0, sizeof(struct fat_info));
    info->partition = partition;
    info->boot_sector = boot_sector;

    if (boot_sector->types.fat_12_16.boot_signature == 0x28 || 
        boot_sector->types.fat_12_16.boot_signature == 0x29
    ) {
        // now we know we are 12/16, find which one is it
        int root_dir_sectors = ((boot_sector->root_entry_count * 32) + boot_sector->bytes_per_sector - 1) / boot_sector->bytes_per_sector;
        int data_sectors = boot_sector->total_sectors_16bits - (boot_sector->reserved_sector_count + (boot_sector->sectors_per_fat_16 * boot_sector->number_of_fats) + root_dir_sectors);
        int data_clusters = data_sectors / boot_sector->sectors_per_cluster;

        // a fat16 cannot have less than 4085 clusters
        info->fat_type = data_clusters < 4085 ? FAT12 : FAT16;
        klog_debug("%s detected", info->fat_type == FAT12 ? "FAT12" : "FAT16");

    } else if (boot_sector->types.fat_32.boot_signature == 0x28 ||
        boot_sector->types.fat_32.boot_signature == 0x29
    ) {
        klog_debug("FAT32 detected");
        info->fat_type = FAT32;
    } else {
        klog_debug("Neither FAT12/16, nor FAT32, unsupported");
        kfree(info);
        kfree(boot_sector);
        return ERR_NO_FAT;
    }

    info->bytes_per_sector = boot_sector->bytes_per_sector;
    info->sectors_per_cluster = boot_sector->sectors_per_cluster;

    info->sectors_per_fat = info->fat_type == FAT32 ? 
        boot_sector->types.fat_32.sectors_per_fat_32 : 
        boot_sector->sectors_per_fat_16;

    info->fat_starting_lba = 
        partition->first_sector + 
        boot_sector->reserved_sector_count;

    // on FAT12/16, the rood dir is fixed size, immediately after the FATs, data clusters start after that
    // on FAT32 we are given a specific cluster number, it resides in data clusters area
    uint32_t end_of_fats_lba = 
            partition->first_sector + 
            boot_sector->reserved_sector_count +
            (info->sectors_per_fat * boot_sector->number_of_fats);
    uint32_t fat1216_root_dir_sectors_size = 
            (boot_sector->root_entry_count * 32) / boot_sector->bytes_per_sector;

    if (info->fat_type == FAT12 || info->fat_type == FAT16) {
        info->data_clusters_starting_lba = end_of_fats_lba + fat1216_root_dir_sectors_size;
        info->root_dir_starting_lba = end_of_fats_lba;
        info->root_dir_sectors_size = fat1216_root_dir_sectors_size;
    } else {
        info->data_clusters_starting_lba = end_of_fats_lba;
        info->root_dir_starting_lba =
            info->data_clusters_starting_lba +
            (boot_sector->types.fat_32.root_dir_cluster - 2) * info->sectors_per_cluster;
        info->root_dir_sectors_size = info->sectors_per_cluster;
    }

    partition->filesys_priv_data = info;
    info->sector_buffer  = kmalloc(info->boot_sector->bytes_per_sector);
    info->fat_buffer     = allocate_consecutive_physical_pages(info->sectors_per_fat * info->boot_sector->bytes_per_sector);
    info->cluster_buffer = allocate_consecutive_physical_pages(info->sectors_per_cluster * info->boot_sector->bytes_per_sector);

    read_fat(info);

    // reading the first cluster of the root dir, for debugging purposes
    info->partition->dev->ops->read(info->partition->dev,
        info->root_dir_starting_lba, 0, 1, info->sector_buffer);
    
    debug_fat_info(info);

    klog_info("FAT contents (excerpt)");
    klog_hex16_info(info->fat_buffer, 
        min(info->sectors_per_fat * info->boot_sector->bytes_per_sector, 128), 
        0);
    klog_info("Root dir contents (first sector only)");
    klog_hex16_info(info->sector_buffer, 512, 0);
    
    struct dir_entry entry;
    int offset = 0;
    uint32_t readme_cluster = 0;
    uint32_t readme_size = 0;
    uint32_t large_cluster = 0;
    uint32_t large_size = 0;

    debug_dir_entry(true, NULL);
    while (true) {
        if (!get_dir_entry(info->sector_buffer, 512, &offset, &entry))
            break;
        debug_dir_entry(false, &entry);

        // let's try to read a file
        if (strcmp(entry.short_name, "README.TXT") == 0) {
            readme_cluster = entry.first_cluster;
            readme_size = entry.file_size;
        } else if (strcmp(entry.short_name, "RAND-LRG.BIN") == 0) {
            large_cluster = entry.first_cluster;
            large_size = entry.file_size;
        }
    }

    if (readme_cluster > 0) {
        read_cluster(info, readme_cluster, info->cluster_buffer);
        klog_debug("Readme file contents:");
        info->cluster_buffer[readme_size] = '\0';
        klog_debug("%s", info->cluster_buffer);
    }

    if (large_cluster > 0) {
        while (large_size > 0) {
            klog_debug("Reading large file, cluster=%d, %d bytes remaining", large_cluster, large_size);
            int err = read_cluster(info, large_cluster, info->cluster_buffer);
            if (err) break;
            klog_debug("Large file excerpt:");
            klog_hex16_info(info->cluster_buffer, 64, 0);

            large_size -= min(large_size, info->bytes_per_sector * info->sectors_per_cluster);
            if (large_size == 0) {
                klog_debug("Size zero, exiting");
                break;

            }

            // we need to find the other cluster.
            large_cluster = get_next_cluster_no(info, large_cluster);
            klog_debug("Next cluster is %d (0x%x)", large_cluster, large_cluster);
            if (large_cluster == 0xFFFFFFFF) {
                klog_debug("no other cluster, exiting");
                break;
            }
        }
    }

    return 0;
}

