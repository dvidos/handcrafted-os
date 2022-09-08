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
#define KERNEL_HEAP_SIZE_KB  2048



// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(multiboot_info_t* mbi, unsigned int boot_magic)
{
    // interrupts are disabled, nmi is also disabled
    // to allow us to set up our Interrupt Table

    // initialize in-memory log
    init_klog();
    klog_appender_level(LOGAPP_MEMORY, LOGLEV_DEBUG);

    // initialize screen and allow logs to be written to it
    screen_init();
    klog_appender_level(LOGAPP_SCREEN, LOGLEV_INFO);
    
    uint32_t heap_start = ((((uint32_t)&kernel_end_address)+4095) & 0xFFFFF000);
    uint32_t heap_end =   ((((uint32_t)&kernel_end_address)+4095) & 0xFFFFF000) + KERNEL_HEAP_SIZE_KB * 1024;

    klog_info("C kernel started");
    // klog_info("  - kernel start address: %4d KB  (0x%x)", (size_t)&kernel_start_address / 1024, (size_t)&kernel_start_address);
    // klog_info("  - kernel end address:   %4d KB  (0x%x)", (size_t)&kernel_end_address / 1024, (size_t)&kernel_end_address);
    // klog_info("  - total kernel size:    %4d KB", ((size_t)&kernel_end_address - (size_t)&kernel_start_address) / 1024);
    klog_info("  Segments                     Size  From        To");
    klog_info("  code (.text)              %4d KB  0x%08x  0x%08x", ((size_t)&kernel_text_size) / 1024, (size_t)&kernel_start_address, (size_t)&kernel_text_end_address);
    klog_info("  ro data (.rodata)         %4d KB  0x%08x  0x%08x", ((size_t)&kernel_rodata_size) / 1024, (size_t)&kernel_text_end_address, (size_t)&kernel_rodata_end_address);
    klog_info("  init data (.data)         %4d KB  0x%08x  0x%08x", ((size_t)&kernel_data_size) / 1024, (size_t)&kernel_rodata_end_address, (size_t)&kernel_data_end_address);
    klog_info("  zero data & stack (.bss)  %4d KB  0x%08x  0x%08x", ((size_t)&kernel_bss_size) / 1024, (size_t)&kernel_data_end_address, (size_t)&kernel_bss_end_address);
    klog_info("  heap                      %4d KB  0x%08x  0x%08x", KERNEL_HEAP_SIZE_KB, heap_start, heap_end);

    if (boot_magic == 0x2BADB002) {
        klog_info("Bootloader info detected, copying it, size of %d bytes", sizeof(multiboot_info_t));
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
    init_physical_memory_manager((void *)&saved_multiboot_info, (void *)&kernel_start_address, &kernel_end_address);

    klog_info("Initializing Serial Port 1 for logging...");
    init_serial_port();

    // maybe tomorrow to a file
    klog_info("Switching logging to serial port");
    klog_appender_level(LOGAPP_SERIAL, LOGLEV_TRACE);
    
    klog_info("Initializing Kernel Heap...");
    init_kernel_heap(KERNEL_HEAP_SIZE_KB * 1024, (void *)heap_start);

    klog_info("Initializing virtual memory mapping...");
    init_virtual_memory_paging(0, (void *)heap_end);

    klog_info("Enabling interrupts & NMI...");
    sti();
    enable_nmi();

    if (strcmp((char *)saved_multiboot_info.cmdline, "tests") == 0) {
        extern void run_tests();
        printk("Running tests: ");
        // klogger must be initialized for tests to report errors
        run_tests();
        printk("\nTests finished, pausing forever...");
        for(;;) asm("hlt");
    }

    klog_info("Detecting PCI devices...");
    ata_register_pci_driver();
    sata_register_pci_driver();
    init_pci();

    klog_info("Initializing file system...");
    discover_storage_dev_partitions(storage_mgr_get_devices_list());
    fat_register_vfs_driver();
    ext2_register_vfs_driver();
    vfs_discover_and_mount_filesystems(get_partitions_list());

    klog_info("Initializing multi-tasking...");
    init_multitasking();

    klog_info("Giving the console to TTY manager...");
    klog_appender_level(LOGAPP_SCREEN, LOGLEV_NONE);
    timer_pause_blocking(250);

    // tty 0 - Alt+1: shell / konsole
    // tty 1 - Alt+2: for running user processes (for now)
    // tty 2 - Alt+3: background seconds timer
    // tty 3 - Alt+4: system monitor (phys memory, paging, heap, processes, devices etc)
    // tty 4 - Alt+5: kernel log
    init_tty_manager(5, 100);

    // now that we have ttys, let's dedicate one to syslog
    klog_set_tty(tty_manager_get_device(4));
    klog_appender_level(LOGAPP_TTY, LOGLEV_INFO);

    // create desired tasks here, 
    void create_some_processes();
    create_some_processes();

    // start_multitasking() will never return
    klog_info("Starting multitasking, goodbye from main()!");
    start_multitasking();
    panic("start_multitasking() returned to main");
}

void shell_launcher() {
    tty_set_title("User Shell");
    klog_info("This is shell launcher");

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
            printf("wait() returned %d, child process exit code is %d\n", err, exit_code);
        }
        proc_sleep(1000);
    }
}

void process_b_main() {
    int i = 1;
    while (true) {
        printf("i = %d\n", i++);
        proc_sleep(2000);
    }
}

void create_some_processes() {
    process_t *proc_a = create_process(
        "Shell Launcher", 
        shell_launcher, 
        PRIORITY_KERNEL,
        0,
        tty_manager_get_device(0)
    );

    // we can trigger a multipage monitor app with memory, processes etc.
    process_t *proc_b = create_process(
        "Task B", 
        process_b_main, 
        PRIORITY_KERNEL,
        0,
        tty_manager_get_device(1)
    );

    start_process(proc_a, false);
    start_process(proc_b, false);
}
