#include <ctypes.h>
#include <bits.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <cpu.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <drivers/clock.h>
#include <drivers/serial.h>
#include <drivers/pci.h>
#include <drivers/sata.h>
#include <drivers/ata.h>
#include <drivers/ramdisk.h>
#include <devices/tty.h>
#include <memory/virtmem.h>
#include <memory/kheap.h>
#include <klog.h>
#include <klib/string.h>
#include <memory/physmem.h>
#include <multiboot.h>
#include <multitask/multitask.h>
#include <multitask/semaphore.h>
#include <multitask/process.h>
#include <multitask/exec.h>
#include <filesys/partition.h>
#include <filesys/vfs.h>
#include <filesys/fat.h>
#include <filesys/ext2.h>
#include <filesys/drivers.h>
#include <filesys/mount.h>
#include <monitor.h>

// Check if the compiler thinks you are targeting the wrong operating system.
#if defined(__linux__)
    #error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

// This tutorial will only work for the 32-bit ix86 targets.
#if !defined(__i686__)
    #error "This tutorial needs to be compiled with a ix86_64-elf compiler"
#endif

// This tutorial will only work for the 32-bit ix86 targets.
#if !defined(__i386__)
    #error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

MODULE("MAIN");

// these are defined in the linker.ld script
// use their *addresses*, not their values!
void kernel_start_address() {}
void kernel_text_end_address() {}
void kernel_rodata_end_address() {}
void kernel_data_end_address() {}
void kernel_bss_end_address() {}
void kernel_end_address() {}

void kernel_text_size() {}
void kernel_data_size() {}
void kernel_rodata_size() {}
void kernel_bss_size() {}
multiboot_info_t saved_multiboot_info;
#define KERNEL_HEAP_SIZE_KB  4096
#define KERNEL_RAMDISK_SIZE_KB  4096


void launch_initial_processes();
void shell_launcher();
void print_multiboot_info(multiboot_info_t *info);



// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(multiboot_info_t* mbi, unsigned int boot_magic)
{
    // interrupts are disabled, nmi is also disabled
    // to allow us to set up our Interrupt Table
    
    uint32_t heap_start = ROUND_UP_4K((uint32_t)&kernel_end_address);
    uint32_t heap_end =   heap_start + KERNEL_HEAP_SIZE_KB * 1024;
    uint32_t ramdisk_start = heap_end;
    uint32_t ramdisk_end = ramdisk_start + KERNEL_RAMDISK_SIZE_KB * 1024;

    // initialize in-memory log
    init_klog();
    klog_appender_level(LOGAPP_MEMORY, LOGLEV_DEBUG);
    // klog_module_level("VFS", LOGLEV_DEBUG);
    // klog_module_level("FAT", LOGLEV_TRACE);
    // klog_module_level("KHEAP", LOGLEV_DEBUG);
    // klog_module_level("VMEM", LOGLEV_TRACE);

    // initialize screen and allow logs to be written to it
    screen_init();
    klog_appender_level(LOGAPP_SCREEN, LOGLEV_INFO);
    
    klog_info("C kernel started");
    klog_info("  Segments                     Size  From        To");
    klog_info("  code (.text)              %4d KB  0x%08x  0x%08x", ((size_t)&kernel_text_size) / 1024, (size_t)&kernel_start_address, (size_t)&kernel_text_end_address);
    klog_info("  ro data (.rodata)         %4d KB  0x%08x  0x%08x", ((size_t)&kernel_rodata_size) / 1024, (size_t)&kernel_text_end_address, (size_t)&kernel_rodata_end_address);
    klog_info("  init data (.data)         %4d KB  0x%08x  0x%08x", ((size_t)&kernel_data_size) / 1024, (size_t)&kernel_rodata_end_address, (size_t)&kernel_data_end_address);
    klog_info("  zero data & stack (.bss)  %4d KB  0x%08x  0x%08x", ((size_t)&kernel_bss_size) / 1024, (size_t)&kernel_data_end_address, (size_t)&kernel_bss_end_address);
    klog_info("  heap                      %4d KB  0x%08x  0x%08x", KERNEL_HEAP_SIZE_KB, heap_start, heap_end);
    klog_info("  ramdisk                   %4d KB  0x%08x  0x%08x", KERNEL_RAMDISK_SIZE_KB, ramdisk_start, ramdisk_end);

    if (boot_magic == 0x2BADB002) {
        klog_info("Bootloader info detected, copying it, size of %d bytes", sizeof(multiboot_info_t));
        print_multiboot_info(mbi);
        memcpy((char *)&saved_multiboot_info, (char *)mbi, sizeof(multiboot_info_t));
    } else {
        klog_warn("No bootloader info detected");
        memset((char *)&saved_multiboot_info, 0, sizeof(multiboot_info_t));
    }

    // kernel code segment selector: 0x08 (8)
    // kernel data segment selector: 0x10 (16)
    klog_info("Initializing Global Descriptor Table...");
    init_gdt();

    klog_info("Initializing Interrupts Descriptor Table...");
    init_idt(0x8);

    klog_info("Initializing Programmable Interrupt Controller...");
    init_pic();

    klog_info("Initializing Programmable Interval Timer...");
    init_timer();

    klog_info("Initializing Real Time Clock...");
    init_real_time_clock(15);

    klog_info("Initializing Physical Memory Manager...");
    init_physical_memory_manager((void *)&saved_multiboot_info, (void *)&kernel_start_address, (void *)ramdisk_end);

    klog_info("Initializing Serial Port 1 for logging...");
    init_serial_port();

    // maybe tomorrow to a file
    klog_info("Switching logging to serial port");
    klog_appender_level(LOGAPP_SERIAL, LOGLEV_TRACE);
    
    klog_info("Initializing Kernel Heap...");
    init_kernel_heap((void *)heap_start, KERNEL_HEAP_SIZE_KB * 1024);

    klog_info("Initializing virtual memory mapping...");
    init_virtual_memory_paging(0, (void *)ramdisk_end);

    klog_info("Enabling interrupts & NMI...");
    sti();
    enable_nmi();

    klog_info("Detecting PCI devices...");
    ata_register_pci_driver();
    sata_register_pci_driver();
    init_pci();

    klog_info("Creating RAM disk...");
    init_ramdisk(ramdisk_start, ramdisk_end);

    klog_info("Initializing file system...");
    klog_module_level("MOUNT", LOGLEV_TRACE);
    discover_storage_dev_partitions(get_storage_devices_list());
    fat_register_vfs_driver();
    ext2_register_vfs_driver();
    vfs_discover_and_mount_filesystems((char *)saved_multiboot_info.cmdline);

    klog_info("Initializing multi-tasking...");
    init_multitasking();

    klog_info("Running tests...");
    klog_module_level("UNITTEST", LOGLEV_DEBUG);
    extern bool run_frameworked_unit_tests();
    if (!run_frameworked_unit_tests()) {
        klog_error("Tests failed, freezing");
        for (;;) asm("hlt");
    }

    klog_info("Giving the console to TTY manager...");
    klog_appender_level(LOGAPP_SCREEN, LOGLEV_NONE);

    // tty 0-3 - Alt+1 through Alt+4: Shell
    // tty 4 - Alt+5: process monitor (memory, heap, processes)
    // tty 5 - Alt+6: filesystem monitor (devices, partitions, mounts)
    // tty 6 - Alt+7: kernel log
    init_tty_manager(7, 100);

    // now that we have ttys, let's dedicate one to syslog
    klog_set_tty(tty_manager_get_device(6));
    klog_appender_level(LOGAPP_TTY, LOGLEV_INFO);

    // create desired tasks here (init, logic, sh, etc)
    launch_initial_processes();

    // start_multitasking() will never return
    klog_info("Starting multitasking, goodbye from main()!");
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
    klog_info("Shell launcher started, PID %d", proc_getpid());
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



void print_multiboot_info(multiboot_info_t *info) {

    // see https://www.gnu.org/software/grub/manual/multiboot/multiboot.html

    // we should print this....
    // to see what GRUB initializes, and do similar stuff
    klog_info("Multiboot info");
    klog_info("- flags: 0x%08x  (%032b)", info->flags, info->flags);
    if (info->flags & MULTIBOOT_INFO_MEMORY) {
        klog_info("- memory lower 0x%x (%u)", info->mem_lower, info->mem_lower);
        klog_info("- memory lower 0x%x (%u)", info->mem_upper, info->mem_upper);
    }
    if (info->flags & MULTIBOOT_INFO_BOOTDEV) {
        klog_info("- root partition 0x%x (drive no, p1, p2, p3)", info->boot_device);
    }
    if (info->flags & MULTIBOOT_INFO_CMDLINE) {
        klog_info("- cmdline \"%s\"", info->cmdline);
    }
    if (info->flags & MULTIBOOT_INFO_MODS) {
        klog_info("- mods address 0x%x, count %d", info->mods_addr, info->mods_count);
    }
    if (info->flags & MULTIBOOT_INFO_AOUT_SYMS) {
        klog_info("- aout info provided");
    } else if (info->flags & MULTIBOOT_INFO_ELF_SHDR) {
        klog_info("- ELF info provided");
        klog_info("  elf n %x, s %u, a %x, i %x", 
            info->u.elf_sec.num,
            info->u.elf_sec.size,
            info->u.elf_sec.addr,
            info->u.elf_sec.shndx
        );
        char *ptr = info->u.elf_sec.addr;
        for (int i = 0; i < info->u.elf_sec.num; i++) {
            // each entry is a Elf32_Shdr structure
            // see https://www.man7.org/linux/man-pages/man5/elf.5.html
            ptr += info->u.elf_sec.size;
        }
    }
    if (info->flags & MULTIBOOT_INFO_MEM_MAP) {
        klog_info("- mmap address 0x%x, length %d", info->mmap_addr, info->mmap_length);
        uint32_t len = 0;
        void *ptr = (char *)info->mmap_addr;
        while (len < info->mmap_length) {
            multiboot_memory_map_t *mm = (multiboot_memory_map_t *)ptr;
            klog_info("  s %d,  a 0x%08x-%08x,  l 0x%08x-%08x,  t %u", 
                mm->size,
                HIGH_DWORD(mm->addr), LOW_DWORD(mm->addr),
                HIGH_DWORD(mm->len), LOW_DWORD(mm->len),
                mm->type);
            len += 24; // sizeof(multiboot_memory_map_t);
            ptr += 24; // sizeof(multiboot_memory_map_t);
        }
    }
    if (info->flags & MULTIBOOT_INFO_DRIVE_INFO) {
        klog_info("- drives address 0x%x, length %d", info->drives_addr, info->drives_length);
    }
    if (info->flags & MULTIBOOT_INFO_CONFIG_TABLE) {
        klog_info("- rom config table 0x%x", info->config_table);
    }
    if (info->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        klog_info("- boot loader name \"%s\"", info->boot_loader_name);
    }
    if (info->flags & MULTIBOOT_INFO_APM_TABLE) {
        klog_info("- apm table address 0x%x", info->apm_table);
    }
    if (info->flags & MULTIBOOT_INFO_VBE_INFO) {
        klog_info("- vbe control info  0x%x", info->vbe_control_info);
        klog_info("- vbe mode info     0x%x", info->vbe_mode_info);
        klog_info("- vbe mode          0x%x", info->vbe_mode);
        klog_info("- vbe interface seg 0x%x", info->vbe_interface_seg);
        klog_info("- vbe interface off 0x%x", info->vbe_interface_off);
        klog_info("- vbe interface len 0x%x", info->vbe_interface_len);
    }
    if (info->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        klog_info("- framebuffer addr   0x%x-%x", HIGH_DWORD(info->framebuffer_addr), LOW_DWORD(info->framebuffer_addr));
        klog_info("- framebuffer pitch  %u", info->framebuffer_pitch);
        klog_info("- framebuffer width  %u", info->framebuffer_width);
        klog_info("- framebuffer height %u", info->framebuffer_height);
        klog_info("- framebuffer bpp    %u", info->framebuffer_bpp);
        klog_info("- framebuffer type   %u", info->framebuffer_type);
    }
}