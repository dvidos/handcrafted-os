#include "fat_priv.h"


static void debug_fat_info(fat_info *fat) {
    klog_debug("FAT info");
    klog_debug("  type                   FAT%d", fat->fat_type == FAT32 ? 32 : (fat->fat_type == FAT16 ? 16 : 12));
    klog_debug("  bytes per sector       %d", fat->bytes_per_sector);
    klog_debug("  sectors per cluster    %d", fat->sectors_per_cluster);
    klog_debug("  cluster size in bytes  %d", fat->sectors_per_cluster * fat->bytes_per_sector);
    klog_debug("  sectors per fat        %d", fat->sectors_per_fat);
    klog_debug("  fat size in bytes      %d", fat->sectors_per_fat * fat->bytes_per_sector);
    klog_debug("  number of fats         %d", fat->boot_sector->number_of_fats);
    klog_debug("  ---");
    klog_debug("  reserved sector count  %d", fat->boot_sector->reserved_sector_count);
    klog_debug("  sectors per fat-16     %d", fat->boot_sector->sectors_per_fat_16);
    klog_debug("  sectors per fat-32     %d", fat->boot_sector->types.fat_32.sectors_per_fat_32);
    klog_debug("  total sectors 16 bits  %d", fat->boot_sector->total_sectors_16bits);
    klog_debug("  total sectors 32 bits  %d", fat->boot_sector->total_sectors_32bits);
    klog_debug("  end-of-chain value     0x%x", fat->end_of_chain_value);
    klog_debug("  ---");
    klog_debug("  partition start lba         %d", fat->partition->first_sector);
    klog_debug("  fat starting lba            %d", fat->fat_starting_lba);
    klog_debug("  data clusters starting lba  %d", fat->data_clusters_starting_lba);
    klog_debug("  root dir starting lba       %d", fat->root_dir_starting_lba);
    klog_debug("  root dir sectors size       %d", fat->root_dir_sectors_count);
    klog_debug("Root dir descriptor:");
    debug_file_descriptor(fat->root_dir_descriptor);
}

static void debug_fat_dir_entry(bool title_line, fat_dir_entry *entry) {
    if (title_line) {
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
            entry->first_cluster_no,
            cstr
        );
    }
}

