#include "types.h"
#include "funcs.h"
#include "bios.h"
#include "memory.h"

void find_and_load_kernel(byte boot_drive, word target_segment) {
    printf("Finding and loading kernel...\n");
    // we should load the partition table (BIOS or EFI)
    // then find the bootable partition
    // then load the superblock of that partition (hoping it is FAT)
    // then find the "boot/kernel" file (or even /kernel.bin in the root directory)
    // then open the file and find the multiboot signature etc.
    // parse the elf headers 
    // and load at 1MB... how?

    // we should have a better and cleaner implimentation of:
    // - bios partitions,
    // - EFI partitions,
    // - FAT filesystem
    // - ELF format

    // for example, 
    // - they should be standalone modules in code, 
    // - they should expose one public header only
    // - they should contain test code
    // - they should have dependencies injected (e.g. reading from disk)

    // /dev/loop101p1 has 2 heads and 32 sectors per track,
    // hidden sectors 0x0800;
    // logical sector size is 512,
    // using 0xf8 media descriptor, with 18432 sectors;
    // drive number 0x80;
    // filesystem has 2 16-bit FATs and 4 sectors per cluster.
    // FAT size is 20 sectors, and provides 4589 clusters.
    // There are 4 reserved sectors.
    // Root directory contains 512 slots and uses 32 sectors.
    // Volume ID is 86c2f835, no volume label.
    
    byte *cluster = malloc(2048); 
    int err;

    // load the first sector to parse partition table
    byte num_heads = 0;
    byte sectors_count = 0;
    err = bios_get_drive_geometry(boot_drive, &num_heads, &sectors_count);
    if (err) {
        printf("Error getting drive 0x%x geometry\n", boot_drive);
    } else {
        // with QEmu I get Drive 0x80, 16 heads, 63 sectors
        printf("Drive 0x%x, %d heads, %d sectors per track\n", boot_drive, num_heads, sectors_count);
    }

    // try to load one thing, then dump it as hex.
    err = bios_load_disk_sectors_lba(boot_drive, 0, 1, cluster);
    if (err) {
        printf("Error reading in LBA, trying CHS\n");
        err = bios_load_disk_sector_chs(boot_drive, 0, 0, 1, cluster);
        if (err) {
            printf("Error reading in CHS mode, giving up\n");
            for(;;) asm ("hlt");
        }
    }

    // try to show it:
    #define p(x)   ((x)>' '&&(x)<='z'?(x):'.')
    for (int i = 0; i < 512; i += 16) {
        printf("0x%04x:  %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
            i,
            p(cluster[i + 0]), p(cluster[i + 1]), p(cluster[i + 2]), p(cluster[i + 3]), 
            p(cluster[i + 4]), p(cluster[i + 5]), p(cluster[i + 6]), p(cluster[i + 7]), 
            p(cluster[i + 8]), p(cluster[i + 9]), p(cluster[i + 10]), p(cluster[i + 11]), 
            p(cluster[i + 12]), p(cluster[i + 13]), p(cluster[i + 14]), p(cluster[i + 15])
        );
    }

    // parse the partition table, first first partition entry

    // load first partition FAT first sector info
    // detect FAT numbers and size, reserved sectors, 
    // find root directory cluster
    
    // iterate through root directory for the "kernel.bin" file

    // start loading it at... say 0x1000:0000 (second or other 64kb segment)
}
