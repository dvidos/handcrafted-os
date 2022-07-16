#include <klib/string.h>
#include <drivers/screen.h>
#include <drivers/keyboard.h>
#include <cpu.h>
#include <errors.h>
#include <bits.h>
#include <multiboot.h>
#include <drivers/clock.h>
#include <drivers/pci.h>
#include <memory/kheap.h>
#include <memory/physmem.h>
#include <konsole/readline.h>
#include <konsole/commands.h>
#include <filesys/vfs.h>
#include <klog.h>
#include <exec.h>



static int do_inb(int argc, char **argv) {
    if (argc < 1) {
        printf("Expecting port as first argument");
        return 1;
    }
    int port = atoi(argv[0]);
    uint8_t value = inb(port);
    printf("0x%x", value);
    return 0;
}

static int do_outb(int argc, char **argv) {
    if (argc < 2) {
        printf("Expecting port as first argument, value as the second\n");
        return 1;
    }
    int port = atoi(argv[0]);
    uint8_t value = atoi(argv[1]) & 0xFF;
    outb(port, value);
    return 0;
}

static int do_inw(int argc, char **argv) {
    if (argc < 1) {
        printf("Expecting port as first argument");
        return 1;
    }
    int port = atoi(argv[0]);
    uint16_t value = inw(port);
    printf("0x%x", value);
    return 0;
}

static int do_outw(int argc, char **argv) {
    if (argc < 2) {
        printf("Expecting port as first argument, value as the second\n");
        return 1;
    }
    int port = atoi(argv[0]);
    uint16_t value = atoi(argv[1]) & 0xFFFF;
    outb(port, value);
    return 0;
}

static int do_print(int argc, char **argv) {
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
        
        int parsed = atoi(argv[i]);
        printf("atoi(\"%s\") --> %d (%bb, 0%o, 0x%x)\n", argv[i], parsed, parsed, parsed, parsed);

        unsigned int uparsed = atoui(argv[i]);
        printf("atoui(\"%s\") --> %u (%bb, 0%o, 0x%x)\n", argv[i], uparsed, uparsed, uparsed, uparsed);
    }
    return 0;
}

static char printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

static int do_mem_dump(int argc, char **argv) {
    if (argc > 2) {
        printf("Expecting address as first argument, length (bytes) as the second\n");
        printf("Address defaults to succeeding previously viewed\n");
        printf("Length defaults to 256 bytes");
        return 1;
    }

    static uint32_t next_address = 0;
    uint32_t address = argc > 0 ? atoui(argv[0]) : next_address;
    uint32_t len = argc == 2 ? atoui(argv[1]) : 256;
    unsigned char *ptr = (unsigned char *)address;
    while (len > 0) {
        // using xxd's format, seems nice
        printf("%08p: %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x  %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
            ptr,
            ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
            ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15],
            printable(ptr[0]), printable(ptr[1]), printable(ptr[2]), printable(ptr[3]),
            printable(ptr[4]), printable(ptr[5]), printable(ptr[6]), printable(ptr[7]),
            printable(ptr[8]), printable(ptr[9]), printable(ptr[10]), printable(ptr[11]),
            printable(ptr[12]), printable(ptr[13]), printable(ptr[14]), printable(ptr[15])
        );
        ptr += 16;
        len -= 16;
    }

    next_address = (uint32_t)ptr;
    return 0;
}

static int do_boot_info(int argc, char **argv) {
    bool all = argc == 0;

    extern multiboot_info_t saved_multiboot_info;
    multiboot_info_t *mbi = &saved_multiboot_info;
    // printf("- flags:       0x%08x\n", mbi->flags);

    if (all && mbi->flags & MULTIBOOT_INFO_MEMORY) {
        printf("Lower Memory:    0 KB - %u KB\n", mbi->mem_lower);
        // the mem_upper is s maximally the address of the first upper memory hole minus 1 megabyte. It is not guaranteed to be this value.
        printf("Upper Memory: 1024 KB - %u KB\n", mbi->mem_upper);
    }

    if (all && mbi->flags & MULTIBOOT_INFO_BOOTDEV) {
        // boot device is four bytes.
        // msb is the drive number (x00 for first floppy, x80 for first hard drive)
        // second msb is the number of partition.
        // third is the partition within the first partition
        // partitions start from zero, unused bytes should be set to xFF
        // DOS extended partitions start form No 4, despite being nested
        printf("Boot drive x%02x (partitions x%02x, x%02x, x%02x)\n",
            (mbi->boot_device >> 24) & 0xFF,
            (mbi->boot_device >> 16) & 0xFF,
            (mbi->boot_device >>  8) & 0xFF,
            (mbi->boot_device >>  0) & 0xFF);
    }

    if (all && mbi->flags & MULTIBOOT_INFO_CMDLINE) {
        printf("Kernel command line: \"%s\"\n", mbi->cmdline);
    }

    if (all && mbi->flags & MULTIBOOT_INFO_MODS) {
        printf("Modules info provided, not shown yet\n");
    }

    if (all && mbi->flags & MULTIBOOT_INFO_AOUT_SYMS) {
        printf("a.out symbols tabsize 0x%x, strsize 0x%x, addr 0x%x\n",
            mbi->u.aout_sym.tabsize,
            mbi->u.aout_sym.strsize,
            mbi->u.aout_sym.addr);
    }

    if (all && mbi->flags & MULTIBOOT_INFO_ELF_SHDR) {
        printf("ELF num 0x%x, size 0x%x, addr 0x%x, shndx 0x%x\n",
            mbi->u.elf_sec.num,
            mbi->u.elf_sec.size,
            mbi->u.elf_sec.addr,
            mbi->u.elf_sec.shndx);
    }

    if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
        if (argc == 1 && strcmp(argv[0], "mmap") == 0) {
            printf("BIOS Memory Map\n");

            uint64_t total_available = 0;
            int size = mbi->mmap_length;
            multiboot_memory_map_t *entry = (multiboot_memory_map_t *)mbi->mmap_addr;
            printf("  Address              Length               Type\n");
            // ntf("  0x12345678:12345678  0x12345678:12345678  Available")
            while (size > 0) {
                char *type;
                switch (entry->type) {
                    case MULTIBOOT_MEMORY_AVAILABLE:
                        type = "Available";
                        break;
                    case MULTIBOOT_MEMORY_RESERVED:
                        type = "Reserved";
                        break;
                    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                        type = "Reclaimable";
                        break;
                    case MULTIBOOT_MEMORY_NVS:
                        type = "NVS";
                        break;
                    case MULTIBOOT_MEMORY_BADRAM:
                        type = "Bad RAM";
                        break;
                    default:
                        type = "Unknown";
                        break;
                }
                printf("  0x%08x:%08x  0x%08x:%08x  %s\n",
                    (uint32_t)(entry->addr >> 32),
                    (uint32_t)(entry->addr & 0xFFFFFFFF),
                    (uint32_t)(entry->len >> 32),
                    (uint32_t)(entry->len & 0xFFFFFFFF),
                    type
                );
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
                    total_available += entry->len;
                size -= sizeof(multiboot_memory_map_t);
                entry++;
            }
            printf("Total available memory %u MB\n", (uint32_t)(total_available / (1024 * 1024)));
        } else {
            printf("Memory map available - use \"mmap\" as first argument to see it\n");
        }
    }

    if (all && mbi->flags & MULTIBOOT_INFO_DRIVE_INFO) {
        printf("Drives info at 0x%08x, %u bytes long\n", mbi->drives_addr, mbi->drives_length);
    }

    if (all && mbi->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        printf("Boot loader \"%s\"\n", mbi->boot_loader_name);
    }

    return 0;
}

static int do_rtc() {
    clock_time_t time;
    get_real_time_clock(&time);

    char *days[] = {"?", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char *months[] = {"?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    printf("The date and time is: %s, %d %s %d, %02d:%02d:%02d\n", 
        days[time.dow], time.days, months[time.months], time.years, 
        time.hours, time.minutes, time.seconds);

    uint32_t secs = get_uptime_in_seconds();
    uint32_t up_days = secs / (24*60*60);
    secs = secs % (24*60*60);
    uint32_t hours = secs / 3600;
    secs = secs % 3600;
    uint32_t mins = secs / 60;
    secs = secs % 60;
    printf("Uptime is %d days, %dh, %dm, %ds\n", up_days, hours, mins, secs);

    return 0;
}

static int do_sizes() {
    printf("sizeof(char)      is %d\n", sizeof(char)); // 2
    printf("sizeof(short)     is %d\n", sizeof(short)); // 2
    printf("sizeof(int)       is %d\n", sizeof(int));   // 4
    printf("sizeof(long)      is %d\n", sizeof(long));  // 4 (was expecting 8!)
    printf("sizeof(long long) is %d\n", sizeof(long long)); // 8
    printf("sizeof(void *) is %d\n", sizeof(void *));  // 4
    printf("sizeof(size_t) is %d\n", sizeof(size_t));  // 4
    
    // advancing a pointer to a struct, advances by the struct size
    // advancing a pointer to a pointer of a struct, advances by a pointer size

    return 0;
}
static int do_kheap() {
    kernel_heap_dump();
    return 0;
}
static int do_phys_mem_dump(int argc, char **argv) {
    if (argc == 0) {
        dump_physical_memory_map_overall();
    } else {
        uint32_t start_address = (uint32_t)atoi(argv[0]);
        dump_physical_memory_map_detail(start_address);
    }
    return 0;
}
static int do_pci_info(int argc, char **argv) {
    extern pci_device_t *pci_devices_list;
    pci_device_t *dev = pci_devices_list;
    while (dev != NULL) {
        printf("%d:%d:%d: vend/dev.id %04x/%04x, class/sub %02x/%02x, hdr %02x\n",
            dev->bus_no,
            dev->device_no,
            dev->func_no,
            dev->config.vendor_id,
            dev->config.device_id,
            dev->config.class_type,
            dev->config.sub_class,
            dev->config.header_type
        );
        printf("        %s, %s\n", 
            get_pci_class_name(dev->config.class_type),
            get_pci_subclass_name(dev->config.class_type, dev->config.sub_class)
        );
        if ((dev->config.header_type & 0x3) == 0) {
            printf("        BARs: %x %x %x %x %x %x\n", 
                dev->config.headers.h00.bar0,
                dev->config.headers.h00.bar1,
                dev->config.headers.h00.bar2,
                dev->config.headers.h00.bar3,
                dev->config.headers.h00.bar4,
                dev->config.headers.h00.bar5
            );
            if (dev->config.headers.h00.interrupt_line != 0
                || dev->config.headers.h00.interrupt_pin != 0) {
                printf("        Interrupt line %d, pin %d\n", 
                    dev->config.headers.h00.interrupt_line,
                    dev->config.headers.h00.interrupt_pin
                );
            }
        }

        dev = dev->next;
    }
    return 0;
}
static int do_mounts() {
    struct mount_info *mount = vfs_get_mounts_list();
    while (mount != NULL) {
        printf("dev #%d, part %d, path=\"%s\", driver=\"%s\"\n", 
            mount->dev->dev_no, 
            mount->part->part_no,
            mount->path,
            mount->driver->name
        );
        mount = mount->next;
    }
    return 0;
}
static int do_ls(int argc, char **argv) {
    char *path = (argc == 0) ? "/" : argv[0];
    file_t f;
    struct dir_entry entry;
    int err;
    err = vfs_opendir(path, &f);
    if (err) {
        printf("vfs_opendir() error %d\n", err);
        return 1;
    }
    while (true) {
        err = vfs_readdir(&f, &entry);
        if (err == ERR_NO_MORE_CONTENT)
            break;
        if (err) {
            printf("vfs_readdir() error %d\n", err);
            return 1;
        }
        printf("%-4s  %04d-%02d-%02d %02d:%02d:%02d  %12d  %s\n", 
            entry.flags.dir ? "dir" : 
                (entry.flags.label ? "vol" : 
                    (entry.flags.read_only ? "RO" : "file")),
            entry.modified.year,
            entry.modified.month,
            entry.modified.day,
            entry.modified.hours,
            entry.modified.minutes,
            entry.modified.seconds,
            entry.file_size, 
            entry.short_name);
    }
    err = vfs_closedir(&f);
    if (err) {
        printf("vfs_closedir() error %d\n", err);
        return 1;
    }
    return 0;
}

static int do_partitions(int argc, char **argv) {
    struct partition *p = get_partitions_list();
    //     " 12   12   12 123456789 123456789  1234.6  Xxxxxx"
    printf("Dev Part Type    Sector   Sectors      MB  Name\n");
    while (p != NULL) {
        printf(" %2d   %2d   %02x %9d %9d  %4d.%d  %s\n",
            p->dev->dev_no,
            p->part_no,
            p->legacy_type,
            p->first_sector,
            p->num_sectors,
            (p->num_sectors * 512) / (1024 * 1024),
            ((p->num_sectors * 512) / (1024 * 100)) % 10,
            p->name
        );
        p = p->next;
    }
    return 0;
}

static int do_cat(int argc, char **argv) {
    if (argc == 0) {
        printf("Usage: cat <file path>\n");
        return 1;
    }
    char *path = argv[0];
    file_t f;
    char buffer[256];
    int err = vfs_open(path, &f);
    if (err) {
        printf("vfs_open() error %d\n", err);
        return 1;
    }
    while (true) {
        int bytes_read = vfs_read(&f, buffer, sizeof(buffer));
        if (bytes_read == 0)
            break;
        if (bytes_read < 0) {
            printf("vfs_read() error %d\n", bytes_read);
            return 1;
        }
        // dumb binary protection
        for (int i = 0; i < bytes_read; i++) {
            char c = (buffer[i] == '\n' || (buffer[i] >= ' ' && buffer[i] <= '~')) ? buffer[i] : '.';
            printf("%c", c);
        }
    }
    err = vfs_close(&f);
    if (err) {
        printf("vfs_close() error %d\n", err);
        return 1;
    }
    printf("\n");
    return 0;
}

static int do_exec(int argc, char **argv) {
    if (argc == 0) {
        printf("Usage: exec <file>\n");
        return 1;
    }
    char *path = argv[0];
    int err = exec(path);
    printf("exec(\"%s\") -> %d", argv[0], err);
    return err;
}

static int do_ascii(int argc, char **argv) {
    printf("Ascii table presentation\n");

    char buffer[80];
    printf("   00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f \n");
    for (int i = 0; i < 16; i++) {
        sprintfn(buffer, sizeof(buffer), "%x0 ", i);
        for (int j = 0; j < 16; j++) {
            unsigned char c = (unsigned char)(i * 16 + j);
            buffer[3 + (j * 3) + 0] = c < 16 ? '.' : c;
            buffer[3 + (j * 3) + 1] = ' ';
            buffer[3 + (j * 3) + 2] = ' ';
        }
        buffer[3 + 16 * 3 + 0] = '\n';
        buffer[3 + 16 * 3 + 1] = '\0';
        printf(buffer);
    }
    return 0;
}

static int do_colors(int argc, char **argv) {
    printf("Console colors\n");
    int old = tty_get_color();

    for (int bg = 0; bg < 16; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            tty_set_color(bg << 4 | fg);
            printf(" Abc");
        }
        printf(" \n");
    }

    tty_set_color(old);
    return 0;
}

int do_touch(int argc, char *argv[]) {
    if (argc == 0) {
        printf("Usage: touch <file>\n");
        return 1;
    }
    int err = vfs_touch(argv[0]);
    if (err)
        printf("Error %d", err);
    return err;
}

int do_mkdir(int argc, char *argv[]) {
    if (argc == 0) {
        printf("Usage: mkdir <name>\n");
        return 1;
    }
    int err = vfs_mkdir(argv[0]);
    if (err)
        printf("Error %d", err);
    return err;
}

int do_unlink(int argc, char *argv[]) {
    if (argc == 0) {
        printf("Usage: unlink <path>\n");
        return 1;
    }
    int err = vfs_unlink(argv[0]);
    if (err)
        printf("Error %d", err);
    return err;
}

// any function can have the argc/argv signature if they want
struct command commands[] = {
    {"print", "Print arguments (to test parsing)", do_print},
    {"inb", "Read byte from port - syntax: inb port", do_inb},
    {"outb", "Write byte to port - syntax: outb port byte", do_outb},
    {"inw", "Read word from port - syntax: inw port", do_inw},
    {"outw", "Write word to port - syntax: outw port word", do_outw},
    // {"mpeek", "Read byte at physical memory - syntax: mpeek address", do_mem_peek},
    // {"mpoke", "Write byte at physical memory - syntax: mpoke address byte", do_mem_poke},
    // {"mpeekw", "Read word at physical memory - syntax: mpeekw address", do_mem_peek},
    // {"mpokew", "Write word at physical memory - syntax: mpokew address word", do_mem_poke},
    {"mdump", "Do hex memory dump - syntax: mdump address length", do_mem_dump},
    // {"hdump", "Do memory heap dump, malloc blocks", do_heap_dump},
    {"bootinfo", "Show information from multiboot spec", do_boot_info},
    // {"pdump", "Show information about OS processes", do_proc_dump},
    {"rtc", "Real Time Clock", do_rtc},
    {"sizes", "Variable sizes in memory", do_sizes},
    {"kheap", "Dump kernel heap", do_kheap},
    {"phys", "Physical Memory Dump", do_phys_mem_dump},
    {"pci", "PCI system info", do_pci_info},
    {"mounts", "Show mounted filesystems", do_mounts},
    {"ls", "Show contents of a directory", do_ls},
    {"partitions", "Show partition information", do_partitions},
    {"cat", "Load and display a file's contents", do_cat},
    {"exec", "Load and execute a file from disk", do_exec},
    {"ascii", "Display ascii table", do_ascii},
    {"colors", "Display available colors", do_colors},
    {"touch", "Create a file", do_touch},
    {"mkdir", "Create a directory", do_mkdir},
    {"unlink", "Delete a file or dir", do_unlink},
    {NULL, NULL, NULL} // last one must be NULL
};


