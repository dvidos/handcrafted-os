#include <ctypes.h>
#include <string.h>
#include <multitask/process.h>
#include <multitask/scheduler.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <drivers/clock.h>
#include <drivers/timer.h>
#include <devices/storage_dev.h>
#include <filesys/partition.h>
#include <filesys/partition.h>
#include <filesys/mount.h>
#include <filesys/drivers.h>


static void show_process(bool title, process_t *p, int *row) {
    tty_set_cursor(*row, 0);
    if (title) {
        printf("  PID  PPID Pr Status     Block Rsn  TTY  PgDir Heap Stack Name");
        //      12345 12345 12 1234567890 1234567890 123 123456 1234  1234 12345678901234567890
        (*row)++;
    } else if (p != NULL) {
        char tty_dev[3+1];
        if (p != NULL && p->tty != NULL)
            sprintfn(tty_dev, sizeof(tty_dev), "%d", tty_get_devno(p->tty));
        else
            strcpy(tty_dev, "-");
        
        printf("%5d %5d %2d %-10s %-10s %3s %6x %4d  %4d %s",
            p->pid,
            p->parent == NULL ? 0 : p->parent->pid,
            p->priority,
            proc_get_status_name(p->state),
            p->state == BLOCKED ? proc_get_block_reason_name(p->block_reason) : "",
            tty_dev,
            p->page_directory,
            p->user_proc.heap_size / 1024,
            p->user_proc.stack_size / 1024,
            p->name
        );
        (*row)++;
    }
}

void show_process_list(proc_list_t *list, int *row) {
    for (process_t *p = list->head; p != NULL; p = p->next)
        show_process(false, p, row);
}

void process_monitor_main() {
    tty_set_title("Process Monitor");

    real_time_clock_info_t time;
    char *days[] = {"?", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char *months[] = {"?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    /*
         top:
        top - 07:34:07 up 10 days, 11:12,  1 user,  load average: 0.46, 0.35, 0.45
        Tasks: 519 total,   1 running, 518 sleeping,   0 stopped,   0 zombie
        %Cpu(s):  0.1 us,  0.2 sy,  0.6 ni, 98.8 id,  0.1 wa,  0.0 hi,  0.2 si,  0.0 st
        MiB Mem :  31435.9 total,  10378.0 free,   8114.0 used,  12943.8 buff/cache
        MiB Swap:   4095.5 total,   4095.5 free,      0.0 used.  22653.8 avail Mem 

 
            PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND                                                                                
           4110 dimitris  25   5 1128.5g 292956 104016 S   6.3   0.9  10:47.57 chrome                                                                                 
           3942 dimitris  25   5   32.4g 144708  92540 S   1.7   0.4  61:24.90 chrome                                                                                 
           4595 dimitris  25   5   20.3g 178232  64084 S   1.0   0.6   8:58.36 code                                                                                   
              1 root      20   0  167940  13420   8196 S   0.0   0.0   0:14.01 systemd                                                                                
              2 root      20   0       0      0      0 S   0.0   0.0   0:00.54 kthreadd                                                                               
              3 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 rcu_gp                                                                                 
              4 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 rcu_par_gp                                                                             
              5 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 netns                                                                                  
  
    */

    while (true) {
        tty_clear();

        // Memory          Total     Free     Used Used        Clock  Abc, 12 Abc 1234, 00:11:22
        // Phys Pages   12345678 12345678 12345678 123%        Uptime 12d 12:23:23m
        // Phys Mem KB  12345678 12345678 12345678 123%        CPU    123%
        // Kern Heap KB 12345678 12345678 12345678 123%    

        get_real_time_clock(&time);
        uint32_t up_secs = get_uptime_in_seconds();
        uint32_t up_days = up_secs / (24*60*60);
        up_secs = up_secs % (24*60*60);
        uint32_t up_hours = up_secs / 3600;
        up_secs = up_secs % 3600;
        uint32_t up_mins = up_secs / 60;
        up_secs = up_secs % 60;
        
        phys_mem_info_t phys_mem;
        get_physical_memory_info(&phys_mem);
        uint32_t phys_mem_kb_percent = (phys_mem.kb_used * 100) / phys_mem.kb_total;
        uint32_t phys_mem_pg_percent = (phys_mem.pages_used * 100) / phys_mem.pages_total;

        uint32_t heap_total = kernel_heap_total_size() / 1024;
        uint32_t heap_free = kernel_heap_free_size() / 1024;
        uint32_t heap_used = heap_total - heap_free;
        uint32_t heap_percent = (heap_used * 100) / heap_total;

        tty_set_cursor(0, 0);
        printf("Memory          Total     Free     Used Used");
        tty_set_cursor(1, 0);
        printf("Phys Mem KB  %8d %8d %8d %3d%%", phys_mem.kb_total, phys_mem.kb_free, phys_mem.kb_used, phys_mem_kb_percent);
        tty_set_cursor(2, 0);
        printf("Phys Pages   %8d %8d %8d %3d%%", phys_mem.pages_total, phys_mem.pages_free, phys_mem.pages_used, phys_mem_pg_percent);
        tty_set_cursor(3, 0);
        printf("Kern Heap KB %8d %8d %8d %3d%%", heap_total, heap_free, heap_used, heap_percent);

        tty_set_cursor(0, 50);
        printf("%3s, %2d %3s %04d, %02d:%02d:%02d",
            days[time.dow], time.days, months[time.months], time.years, 
            time.hours, time.minutes, time.seconds
        );

        tty_set_cursor(1, 50);
        printf("Uptime %2dd %02dh %02dm %02ds", up_days, up_hours, up_mins, up_secs);

        // we shouldn't dive into multitasking internals, but how? 
        int row = 5;
        show_process(true, NULL, &row);
        show_process(false, running_process(), &row);
        for (int i = 0; i < PROCESS_PRIORITY_LEVELS; i++)
            show_process_list(&ready_lists[i], &row);
        show_process_list(&blocked_list, &row);
        show_process_list(&terminated_list, &row);
 
        proc_sleep(1500);
    }
}


void vfs_monitor_main() {
    tty_set_title("VFS Monitor");

    while (true) {
        tty_clear();
        int row = 0;


        tty_set_cursor(row++, 0);
        printf("---------- Storage Devices ----------");
        tty_set_cursor(row++, 0);
        printf("DevNo Name                          ");
        //     |  n   123456789012345678901234567890
        struct storage_dev *dev = get_storage_devices_list();
        while (dev != NULL) {
            tty_set_cursor(row++, 0);
            printf(" %2d   %s", dev->dev_no, dev->name);
            dev = dev->next;
        }
        row++;

        tty_set_cursor(row++, 0);
        printf("---------- Logical Volumes ----------");
        tty_set_cursor(row++, 0);
        printf("Dev Part Tp B Name                                      Sectors  1st    count");
        //     | n    n  XX * 123456789012345678901234567890123456789012345 12345678 12345678
        struct partition *part = get_partitions_list();
        while (part != NULL) {
            tty_set_cursor(row++, 0);
            printf("%2d   %2d  %02x %c %-45s %8d %8d",
                part->dev->dev_no,
                part->part_no,
                part->legacy_type,
                part->bootable ? 'B' : ' ',
                part->name,
                part->first_sector,
                part->num_sectors
            );
            part = part->next;
        }
        row++;


        tty_set_cursor(row++, 0);
        printf("---------- Mounted Filesystems ----------");
        tty_set_cursor(row++, 0);
        printf("Dev Part Driver     Mount point");
        //     | n    n  1234567890 123456789012345678901234567890
        struct mount_info *mnt = vfs_get_mounts_list();
        while (mnt != NULL) {
            tty_set_cursor(row++, 0);
            printf("%2d   %2d  %-10s %s",
                mnt->dev->dev_no,
                mnt->part->part_no,
                mnt->driver->name,
                mnt->path
            );
            mnt = mnt->next;
        }
        row++;

        proc_sleep(2000);
    }
}
