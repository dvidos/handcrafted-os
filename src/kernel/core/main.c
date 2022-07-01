#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <cpu.h>
#include <drivers/screen.h>
#include <drivers/keyboard.h>
#include <drivers/timer.h>
#include <drivers/clock.h>
#include <drivers/serial.h>
#include <drivers/pci.h>
#include <drivers/sata.h>
#include <drivers/ata.h>
#include <memory/physmem.h>
#include <memory/kheap.h>
#include <klog.h>
#include <klib/string.h>
#include <multiboot.h>
#include <multitask/multitask.h>
#include <multitask/semaphore.h>
#include <multitask/process.h>
#include <konsole/konsole.h>
#include <devices/tty.h>
#include <konsole/readline.h>
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


// these two defined in the linker.ld script
// use their *addresses*, not their values!
uint8_t kernel_start_address;
uint8_t kernel_end_address;
multiboot_info_t saved_multiboot_info;

void process_a_main();
void process_b_main();
void process_c_main();



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
    
    klog_info("C kernel started, loaded at 0x%x - 0x%x (%u KB)",
        (uint32_t)&kernel_start_address,
        (uint32_t)&kernel_end_address,
        ((uint32_t)&kernel_end_address - (uint32_t)&kernel_start_address) / 1024
    );

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
    init_physical_memory_manager(&saved_multiboot_info, &kernel_start_address, &kernel_end_address);

    klog_info("Initializing Serial Port 1 for logging...");
    init_serial_port();

    // maybe tomorrow to a file
    klog_info("Switching logging to serial port");
    klog_appender_level(LOGAPP_SERIAL, LOGLEV_TRACE);
    
    klog_info("Initializing Kernel Heap...");
    init_kernel_heap();

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

    // 0: shell / konsole
    // 1: system monitor
    // 2: something
    // 3: kernel log
    init_tty_manager(4, 100);

    // now that we have ttys, let's dedicate one to syslog
    klog_set_tty(tty_manager_get_device(3));
    klog_appender_level(LOGAPP_TTY, LOGLEV_DEBUG);

    // create desired tasks here, 
    start_process(create_process(process_a_main, "Task A", 2, tty_manager_get_device(0)));
    start_process(create_process(process_b_main, "Task B", 2, tty_manager_get_device(1)));
    start_process(create_process(process_c_main, "System Monitor", 2, tty_manager_get_device(2)));

    // start_multitasking() will never return
    klog_info("Starting multitasking, goodbye from main()!");
    start_multitasking();
    panic("start_multitasking() returned to main");
}


void process_a_main() {
    tty_t *tty = tty_manager_get_device(0);
    tty_set_title("Kernel Console");
    tty_write("Welcome to konsole, enter \"?\" for help");
    konsole(tty);
}

void process_b_main() {
    tty_t *tty = tty_manager_get_device(1);
    tty_set_title("Numbers counting on red backgroudn");
    char buffer[32];
    char *message = "Hello from process A! - use Ctrl+Alt+Fn to switch ttys";
    tty_write(message);


    int i = 0;
    tty_set_color(VGA_COLOR_RED << 4 | VGA_COLOR_WHITE);
    while (true) {
        sprintfn(buffer, sizeof(buffer), "\nTask A, i=%d...", i++);
        tty_write(buffer);
        sleep(1000);

        if (i % 60 == 0)
            klog_info("Just fyi, i is %d", i);
    }
}

void process_c_main() {
    tty_t *tty = tty_manager_get_device(2);
    tty_set_title("Ascii table presentation");

    tty_set_color(VGA_COLOR_BLUE << 4 | VGA_COLOR_WHITE);
    char buffer[80];
    tty_write("   00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f \n");
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
        tty_write(buffer);
    }
}

void isr_handler(registers_t regs) {
    // don't forget we have mapped IRQs 0+ to 0x20+
    // to avoid the first 0x1F interrupts that are CPU faults in protected mode
    switch (regs.int_no) {
        case 0x20:
            timer_interrupt_handler(&regs);
            multitasking_timer_ticked();
            break;
        case 0x21:
            keyboard_handler(&regs);
            break;
        case 0x28:
            real_time_clock_interrupt_interrupt_handler(&regs);
            break;
        case 0x100:
            klog_warn("Received interrupt %d (0x%x) -- possible syscall?", regs.int_no, regs.int_no);
            // should return 56 somewhere
            __asm__("mov $56, %eax");
            break;
        default:
            klog_warn("Received interrupt %d (0x%x), error %d", regs.int_no, regs.int_no, regs.err_code);
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(regs.int_no);
}
