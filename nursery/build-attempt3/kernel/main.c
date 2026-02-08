#include <ctypes.h>
#include <bits.h>
#include "utils/panic.h"
#include "misc/gdt.h"
#include "misc/idt.h"
#include "misc/pic.h"
#include "misc/cpu.h"
#include "drivers/screen.h"
#include "drivers/timer.h"
#include "drivers/clock.h"
#include "drivers/serial.h"
#include "drivers/pci.h"
#include "drivers/sata.h"
#include "drivers/ata.h"
#include "drivers/ramdisk.h"
#include "devices/tty.h"
#include "memory/virtmem.h"
#include "memory/kheap.h"
#include "utils/logger.h"
#include "klib/string.h"
#include "memory/physmem2.h"
#include "multitask/multitask.h"
#include "multitask/semaphore.h"
#include "multitask/process.h"
#include "multitask/exec.h"
#include "filesys/partition.h"
#include "filesys/vfs.h"
#include "filesys/fat.h"
#include "filesys/drivers.h"
#include "filesys/mount.h"
#include "processes/monitor.h"
#include "../stage2/boot_info.h" // passed from 2nd stage
#include "../config.h" // compilation configuration



// Check if the compiler thinks you are targeting the wrong operating system.
#if defined(__linux__)
    #error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

// This tutorial will only work for the 32-bit ix86 targets.
#if !defined(__i386__)
    #error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

MODULE("MAIN");



void launch_initial_processes();
void shell_launcher();
void initialize_physical_memory(boot_info_t *info);
void print_stage2_boot_info(boot_info_t *info);

boot_info_t saved_multiboot_info;

typedef struct mem_chunk_t {
    char name[32];
    phys_addr_t start;
    size_t length;
} mem_chunk_t;

#define KERNEL_CHUNKS 7
static mem_chunk_t kernel_chunks[KERNEL_CHUNKS];





// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(boot_info_t* boot)
{
    // interrupts are disabled, nmi is also disabled
    // to allow us to set up our Interrupt Table

    // initialize in-memory log
    init_logger();
    logger_set_appender_log_level(LOG_APPENDER_MEMORY, LOG_LEVEL_DEBUG);
    // logger_set_module_log_level("VFS", LOG_LEVEL_DEBUG);
    // logger_set_module_log_level("FAT", LOG_LEVEL_TRACE);
    // logger_set_module_log_level("KHEAP", LOG_LEVEL_DEBUG);
    // logger_set_module_log_level("VMEM", LOG_LEVEL_TRACE);

    // initialize screen and allow logs to be written to it
    screen_init();
    logger_set_appender_log_level(LOG_APPENDER_SCREEN, LOG_LEVEL_DEBUG);
    panic_set_writer(screen_panic_writer);
    
    log_info("Kernel starting");
    log_info("Version %s, (%s), built %s", VERSION, GIT_HASH, DATE_BUILT);

    print_stage2_boot_info(boot);
    memcpy((char *)&saved_multiboot_info, (char *)boot, sizeof(boot_info_t));

    // kernel code segment selector: 0x08 (8)
    // kernel data segment selector: 0x10 (16)
    log_info("Initializing Global Descriptor Table...");
    init_gdt();

    log_info("Initializing Interrupts Descriptor Table...");
    init_idt(0x8);

    log_info("Initializing Programmable Interrupt Controller...");
    init_pic();

    log_info("Initializing Programmable Interval Timer...");
    init_timer();

    log_info("Initializing Real Time Clock...");
    init_real_time_clock(15);

    log_info("Initializing Physical Memory Manager...");
    initialize_physical_memory(boot);

    log_info("Initializing Serial Port 1 for logging...");
    init_serial_port();

    log_info("Switching logging to serial port");
    logger_set_appender_log_level(LOG_APPENDER_SERIAL, LOG_LEVEL_TRACE);
    panic_set_writer(serial_panic_writer);
    
    log_info("Initializing Kernel Heap...");
    init_kernel_heap((void *)KERNEL_HEAP_ADDRESS, KERNEL_HEAP_SIZE_KB * 1024);

    log_info("Initializing virtual memory mapping...");
    init_virtual_memory_paging(0, (void *)pmm.get_top_identity_address());

    log_info("Enabling interrupts & NMI...");
    sti();
    enable_nmi();

    log_info("Detecting PCI devices...");
    ata_register_pci_driver();
    sata_register_pci_driver();
    init_pci();

    log_info("Creating RAM disk...");
    init_ramdisk(KERNEL_RAMDISK_ADDRESS, KERNEL_RAMDISK_SIZE_KB * 1024);

    log_info("Initializing file system...");
    logger_set_module_log_level("MOUNT", LOG_LEVEL_TRACE);
    discover_storage_dev_partitions(get_storage_devices_list());
    fat_register_vfs_driver();
    vfs_discover_and_mount_filesystems((char *)saved_multiboot_info.cmdline);

    panic("Pausing here, until there is a FS to mount the file system");

    log_info("Initializing multi-tasking...");
    init_multitasking();

    log_info("Giving the console to TTY manager...");
    logger_set_appender_log_level(LOG_APPENDER_SCREEN, LOG_LEVEL_NONE);

    // tty 0-3 - Alt+1 through Alt+4: Shell
    // tty 4 - Alt+5: process monitor (memory, heap, processes)
    // tty 5 - Alt+6: filesystem monitor (devices, partitions, mounts)
    // tty 6 - Alt+7: kernel log
    init_tty_manager(7, 100);

    // now that we have ttys, let's dedicate one to syslog
    logger_set_tty(tty_manager_get_device(6));
    logger_set_appender_log_level(LOG_APPENDER_TTY, LOG_LEVEL_INFO);

    // create desired tasks here (init, logic, sh, etc)
    launch_initial_processes();

    // start_multitasking() will never return
    log_info("Starting multitasking, goodbye from main()!");
    start_multitasking();
    panic("start_multitasking() returned to main");
}

void launch_initial_processes() {
    
    // this would be surpassed by /etc/initrc at some point
    
    int tty;
    process_t *proc;
    int pri = PRIORITY_USER_PROGRAM;

    for (tty = 0; tty < 4; tty++) {
        proc = create_process("Shell Launcher", shell_launcher, pri, 0, tty_manager_get_device(tty));
        start_process(proc);
    }

    proc = create_process("Process Monitor", process_monitor_main, pri, 0, tty_manager_get_device(tty++));
    start_process(proc);

    proc = create_process("VFS Monitor", vfs_monitor_main, pri, 0, tty_manager_get_device(tty++));
    start_process(proc);
}

void shell_launcher() {
    log_info("Shell launcher started, PID %d", proc_getpid());
    tty_set_title("Shell");

    while (true) {
        tty_write("Launching user-space shell program\n");
        int err = exec("/bin/sh");
        if (err < 0) {
            printf("exec(\"/bin/sh\") returned %d\n", err);
        } else {
            // wait for the child?
            pid_t child_proc = (pid_t)err;
            int exit_code = 0;
            err = proc_wait_child(&exit_code);
            printf("Shell exit code was %d\n", exit_code);
        }
        proc_sleep(3000);
    }
}

// these are defined in the linker.ld script
// use their *addresses*, not their values!
void _linker_start_address() {}
void _segment_text_start() {}
void _segment_text_end() {}
void _segment_rodata_start() {}
void _segment_rodata_end() {}
void _segment_init_data_start() {}
void _segment_init_data_end() {}
void _segment_zero_data_start() {}
void _segment_zero_data_end() {}
void _linker_end_address() {}


static inline void register_kernel_chunk(int *num, const char *name, phys_addr_t addr, size_t size) {
    strcpy(kernel_chunks[*num].name, name);
    kernel_chunks[*num].start = addr;
    kernel_chunks[*num].length = size;
    (*num)++;
}

void initialize_physical_memory(boot_info_t *info) {

    // find highest memory address of machine, cap at 4GB
    uint64_t machine_max_memory_64 = 0;
    for (uint32_t i = 0; i < info->mem.count; i++) {
        uint64_t entry_top64 = info->mem.entries[i].base + info->mem.entries[i].length;
        if (entry_top64 > machine_max_memory_64)
            machine_max_memory_64 = entry_top64;
    }

    int i = 0;
    register_kernel_chunk(&i, "text",          (phys_addr_t)&_segment_text_start,      (size_t)(_segment_text_end      - _segment_text_start));
    register_kernel_chunk(&i, "ro_data",       (phys_addr_t)&_segment_rodata_start,    (size_t)(_segment_rodata_end    - _segment_rodata_start));
    register_kernel_chunk(&i, "init_data",     (phys_addr_t)&_segment_init_data_start, (size_t)(_segment_init_data_end - _segment_init_data_start));
    register_kernel_chunk(&i, "zero_data/bss", (phys_addr_t)&_segment_zero_data_start, (size_t)(_segment_zero_data_end - _segment_zero_data_start));
    register_kernel_chunk(&i, "stack",         (phys_addr_t)_segment_zero_data_end, (size_t)(KERNEL_STACK_TOP - (size_t)&_segment_zero_data_end));
    register_kernel_chunk(&i, "heap",          (phys_addr_t)KERNEL_HEAP_ADDRESS, (size_t)KERNEL_HEAP_SIZE_KB * 1024);
    register_kernel_chunk(&i, "ramdisk",       (phys_addr_t)KERNEL_RAMDISK_ADDRESS, (size_t)KERNEL_RAMDISK_SIZE_KB * 1024);

    // where physical memory mapper can put its bitmap
    phys_addr_t kernel_top_address = 0;
    for (int i = 0; i < KERNEL_CHUNKS; i++) {
        phys_addr_t chunk_top = kernel_chunks[i].start + kernel_chunks[i].length;
        if (chunk_top > kernel_top_address)
            kernel_top_address = chunk_top;
    }

    log_info("Machine maximum memory address 0x%08x.%08x (%u KB, %u MB, %u GB)",
        (uint32_t)(machine_max_memory_64 >> 32),
        (uint32_t)(machine_max_memory_64 & 0xFFFFFFFF),
        (uint32_t)(machine_max_memory_64 / 1024),
        (uint32_t)(machine_max_memory_64 / (1024 * 1024)),
        (uint32_t)(machine_max_memory_64 / (1024 * 1024 * 1024))
    );
    log_info("Kernel area topmost address 0x%08x", kernel_top_address);
    log_info("Kernel memory             From          To   From KB     To KB   Size KB");
    //         - 1234567890123456  0x12345678  0x12345678 123456789 123456789   1234567
    for (int i = 0; i < KERNEL_CHUNKS; i++) {
        mem_chunk_t *chunk = &kernel_chunks[i];
        log_info("- %-16s  0x%08x  0x%08x %9u %9u   %7u",
            chunk->name,
            chunk->start,
            chunk->start + chunk->length,
            chunk->start / 1024,
            (chunk->start + chunk->length) / 1024,
            chunk->length / 1024
        );
    }

    pmm.initialize(machine_max_memory_64, kernel_top_address);
    for (uint32_t i = 0; i < info->mem.count; i++) {
        e820_memory_entry *entry = &info->mem.entries[i];
        pmm.mark_region_available((phys_addr_t)entry->base, (size_t)entry->length);
    }
    pmm.mark_region_reserved((phys_addr_t)0, (size_t)kernel_top_address);
    pmm.finish_initialization();

    log_info("Physical memory manager initialized. %u total pages, %u (%u KB or %u%%) reserved, %u (%u KB or %u%%) available",
        pmm.total_pages(),
        pmm.used_pages(),
        pmm.used_pages() * 4,
        pmm.total_pages() == 0 ? 0 : (pmm.used_pages() * 100) / pmm.total_pages(),
        pmm.free_pages(),
        pmm.free_pages() * 4,
        pmm.total_pages() == 0 ? 0 : (pmm.free_pages() * 100) / pmm.total_pages()
    );

    pmm.debug_bitmap_ranges();
}

void print_stage2_boot_info(boot_info_t *info) {
    log_info("boot_info from stage 2 (ptr address 0x%08x)", info);
    log_info("- cmd line: \"%s\"", info->cmdline);
    log_info("- memory map (total of %u entries)", info->mem.count);
    log_info("    No              Address               Length  Type  ACPI");
    //             00  0x12345678-12345678  0x12345678-12345678  1234  1234
    for (uint32_t i = 0; i < info->mem.count; i++) {
        e820_memory_entry *mm = &info->mem.entries[i];
        log_info("    %2u  0x%08x-%08x  0x%08x-%08x  %4u  %4u", 
            i,
            HIGH_DWORD(mm->base), LOW_DWORD(mm->base),
            HIGH_DWORD(mm->length), LOW_DWORD(mm->length),
            mm->type, 
            mm->acpi_ext);
    }
    log_info("- VBE framebuffer at 0x%08x-%08x, %u x %u x %u, pitch %u", 
        HIGH_DWORD(info->fb.fb_addr),
        LOW_DWORD(info->fb.fb_addr),
        info->fb.width,
        info->fb.height,
        info->fb.bpp,
        info->fb.pitch);
}
