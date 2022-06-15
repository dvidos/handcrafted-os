#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <klog.h>
#include <klib/string.h>

// mainly based on https://www.pjrc.com/tech/8051/ide/fat32.html
// and https://wiki.osdev.org/FAT

// FAT is so weird!
// it uses clusters for addressing, not sectors.
// clusters are usually 4K, 8K, 16K or 32K long
// the MBR sector gives us the first cluster of the root directory.
// each directory entry contains the name and the first cluster of the file/dir
// to find next entry of a directory or file, we need to read the FAT, by
//    indexing it into 12-, 16- or 32-bit integers,
//    reading the entry that corresponds to the first file cluster
//    and the value is the number of the next cluster (4 MS bits should be cleared)


enum FAT_TYPE { FAT_32, FAT_16, FAT_12 };

struct fat_info {
    struct partition *partition;
    // type FAT12, 16, 32
    // volume label
    // offsets etc.
    enum FAT_TYPE fat_type;

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  number_of_fats;
    uint32_t sectors_per_fat;
    uint32_t root_dir_first_cluster;

    // absolute addressing, relative to disk
    uint32_t fat_starting_lba;
    uint32_t clusters_starting_lba;

    // allocated bufers for reading the fat and files
    uint8_t *fat_buffer;
    uint8_t *cluster_buffer;
};

uint32_t get_next_cluster_no(struct fat_info *info, uint32_t current_cluster_no) {

    if (info->fat_type == FAT_12) {
        // multiply by 1.5 is 1 + (1/2), rounding down
        uint32_t offset = current_cluster_no + (current_cluster_no / 2);
        uint16_t word = *(uint16_t *)&info->fat_buffer[offset];

        // see if we rounded down
        if (current_cluster_no & 0x0001) {
            word >>= 4; // use three higher nibbles
        } else {
            word &= 0xFFF; // use three lower nibbles
        }
    } else if (info->fat_type == FAT_16) {
        uint32_t offset = current_cluster_no * 2; // two bytes for 16 bits
        uint32_t next_cluster_no = *(uint16_t *)&info->fat_buffer[offset];
        return next_cluster_no;
    } else if (info->fat_type == FAT_32) {
        uint32_t offset = current_cluster_no * 4; // four bytes for 32 bits
        uint32_t next_cluster_no = *(uint32_t *)&info->fat_buffer[offset];
        return next_cluster_no;
    }

    // else return none
    return 0xFFFFFFFF;
}

int read_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer) {
    return info->partition->dev->ops->read(
        info->partition->dev,
        info->clusters_starting_lba + ((cluster_no - 2) * info->sectors_per_cluster),
        0,
        info->sectors_per_cluster,
        buffer
    );
}

int write_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer) {
    return -1;
}

int read_fat(struct fat_info *info) {
    return info->partition->dev->ops->read(
        info->partition->dev,
        info->fat_starting_lba,
        0,
        info->sectors_per_fat,
        info->fat_buffer
    );
}

int write_fat(struct fat_info *info) {
    // we should write twice
    return -1;
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
    klog_debug("Checking partition #%d for FAT (legacy type 0x%02x)", partition->part_no, partition->legacy_type);

    // the legacy partition type (0B or 0C) is not a great indicator,
    // when i formatted an image under linux, the type was 83 (linux fs)
    
    uint8_t *page = allocate_physical_page();
    // let's check the first sector of the partition.
    int err = partition->dev->ops->read(partition->dev, 
        partition->first_sector, 
        0,
        1,
        page
    );
    if (err) {
        klog_debug("Err %d reading first sector of partition", err);
        return err;
    }
    
    // klog_info("MBR sector");
    // klog_hex16_info(page, 512, 0);
    if (page[0x1FE] != 0x55 || page[0x1FF] != 0xAA) {
        klog_debug("Not a FAT MBR...");
        return ERR_NO_FAT;
    }

    uint16_t bytes_per_sector    = *(uint16_t *)&page[0x0B]; // should be 512
    uint8_t  sectors_per_cluster = *(uint8_t  *)&page[0x0D];
    uint16_t reserved_sectors    = *(uint16_t *)&page[0x0E];
    uint8_t  number_of_fats      = *(uint8_t  *)&page[0x10]; // should be 2
    uint32_t sectors_per_fat     = *(uint32_t *)&page[0x24];
    uint32_t root_dir_cluster    = *(uint32_t *)&page[0x2C];

    klog_debug(
        "sectors_per_cluster=%d, reserved_sectors=%d, sectors_per_fat=%d, root_dir_cluster=%d",
        sectors_per_cluster,
        reserved_sectors,
        sectors_per_fat,
        root_dir_cluster
    );
    
    if (bytes_per_sector != 512 
        || number_of_fats != 2
    ) {
        klog_debug("Heuristics don't think this is a FAT...");
        return ERR_NO_FAT;
    }

    klog_info("Seems like a FAT to me!");
    struct fat_info *fat_info = kmalloc(sizeof(struct fat_info));
    memset(fat_info, 0, sizeof(struct fat_info));

    fat_info->partition = partition;
    fat_info->fat_type = FAT_32; // we only support this for now

    fat_info->bytes_per_sector    = bytes_per_sector;
    fat_info->sectors_per_cluster = sectors_per_cluster;
    fat_info->reserved_sectors    = reserved_sectors;
    fat_info->number_of_fats      = number_of_fats;
    fat_info->sectors_per_fat     = sectors_per_fat;

    fat_info->fat_starting_lba = partition->first_sector + reserved_sectors;
    fat_info->clusters_starting_lba = 
            partition->first_sector + 
            reserved_sectors + 
            (sectors_per_fat * number_of_fats);
    fat_info->root_dir_first_cluster = root_dir_cluster;
    partition->filesys_priv_data = fat_info;
    fat_info->cluster_buffer = allocate_consecutive_physical_pages(bytes_per_sector * sectors_per_cluster);
    fat_info->fat_buffer = allocate_consecutive_physical_pages(bytes_per_sector * sectors_per_fat);

    read_fat(fat_info);
    read_cluster(fat_info, fat_info->root_dir_first_cluster, fat_info->cluster_buffer);
    
    klog_info("FAT contents");
    klog_hex16_info(fat_info->fat_buffer, fat_info->sectors_per_fat * fat_info->bytes_per_sector, 0);
    klog_info("Root dir contents (first cluster)");
    klog_hex16_info(fat_info->cluster_buffer, fat_info->sectors_per_cluster * fat_info->bytes_per_sector, 0);
    
    // for (int c = 0; c < 45; c++) {
    //     read_cluster(fat_info, c, fat_info->cluster_buffer);
    //     klog_hex16_info(fat_info->cluster_buffer, fat_info->sectors_per_cluster * fat_info->bytes_per_sector, 0);
    // }

    return 0;
}

