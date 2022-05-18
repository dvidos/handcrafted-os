#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "screen.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "cpu.h"
#include "keyboard.h"
#include "timer.h"
#include "clock.h"
#include "serial.h"
#include "physmem.h"
#include "kheap.h"
#include "string.h"
#include "multiboot.h"
#include "konsole.h"
#include "process.h"
#include "klog.h"



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

mutex_t shared_memory_mutex;
char shared_memory[256];

void process_a_main();
void process_b_main();
void process_c_main();
void process_d_main();


// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(multiboot_info_t* mbi, unsigned int boot_magic)
{
    // initialize in-memory log
    init_klog();

    // initialize screen and allow logs to be written to it
    screen_init();
    klog_screen(true);

    klog("C kernel kicking off, loaded at 0x%x - 0x%x (size of %u KB)\n",
        (uint32_t)&kernel_start_address,
        (uint32_t)&kernel_end_address,
        ((uint32_t)&kernel_end_address - (uint32_t)&kernel_start_address) / 1024
    );

    // it seems interrupts are disabled, nmi is also disabled
    // klog("Interrupts enabled? %d\n", interrupts_enabled());
    // klog("NMI is enabled? %d\n", nmi_enabled());

    if (boot_magic == 0x2BADB002) {
        klog("Bootloader info detected, copying it, size of %d bytes\n", sizeof(multiboot_info_t));
        memcpy((char *)&saved_multiboot_info, (char *)mbi, sizeof(multiboot_info_t));
    } else {
        klog("No bootloader info detected\n");
        memset((char *)&saved_multiboot_info, 0, sizeof(multiboot_info_t));
    }

    // code segment selector: 0x08 (8)
    // data segment selector: 0x10 (16)
    klog("Initializing Global Descriptor Table...\n");
    init_gdt();

    klog("Initializing Interrupts Descriptor Table...\n");
    init_idt(0x8);

    klog("Initializing Programmable Interrupt Controller...\n");
    init_pic();

    klog("Initializing Programmable Interval Timer...\n");
    init_timer();

    klog("Initializing Real Time Clock...\n");
    init_real_time_clock(15);

    klog("Initializing Physical Memory Manager...\n");
    init_physical_memory_manager(&saved_multiboot_info, &kernel_start_address, &kernel_end_address);

    klog("Initializing Serial Port 1 for logging...\n");
    init_serial_port();

    // maybe tomorrow to a file
    klog("Switching logging to serial port\n");
    klog_serial_port(true);
    // klog_screen(false); -- disabling this we cannot work on our old laptop
    
    klog("Initializing Kernel Heap...\n");
    init_kernel_heap();

    klog("Enabling interrupts & NMI...\n");
    sti();
    enable_nmi();

    if (strcmp((char *)saved_multiboot_info.cmdline, "tests") == 0) {
        printf("Running tests...\n");
        extern void run_tests();
        run_tests();
        screen_write("Tests finished, pausing forever...");
        for(;;)
            asm("hlt");
    } else if (strcmp((char *)saved_multiboot_info.cmdline, "console") == 0) {
        printf("Starting konsole...\n");
        konsole();
    }

    // klog("Testing the hard disks...\n");
    // extern void test_hdd();
    // test_hdd();
    // for(;;) asm("hlt");

    klog("Initializing multi-tasking...\n");
    init_multitasking();

    // define and grab the mutex, we should be all alone
    memset(&shared_memory_mutex, 0, sizeof(shared_memory_mutex));
    shared_memory_mutex.limit = 1;

    // create desired tasks here, 
    start_process(create_process(process_a_main, "Task A", 2));
    start_process(create_process(process_b_main, "Task B", 1));
    start_process(create_process(process_c_main, "Task C", 1));
    start_process(create_process(process_d_main, "Task D", 1));

    // start_multitasking() will never return
    klog("Starting multitasking, goodbye from main()!\n");
    start_multitasking();
    
    panic("start_multitasking() returned to main");
}


void process_a_main() {
    // sleep a bit, to avoid race condition with other tasks
    sleep(50);

    klog("A: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex); // we should be the ones
    klog("A: acquired mutex\n");

    klog("A: writing shared memory\n");
    memset(shared_memory, 0, sizeof(shared_memory));
    strcpy(shared_memory, "Hi there from task A!");

    // give time to others to wake and block, while we hold the semaphore
    klog("A: sleeping with acquired mutex\n");
    sleep(200);


    klog("A: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    klog("A: released mutex\n");

    // pausing for a bit, then will read it.
    sleep(2000);

    klog("A: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex);
    klog("A: acquired mutex\n");
    klog("A: shared memory contents: [%s]\n", shared_memory);
    klog("A: releasing and exiting\n");
    release_mutex(&shared_memory_mutex);
    //kernel_heap_dump();
    dump_process_table();
}

void process_b_main() {
    // sleep longer, to avoid race condition with other tasks
    sleep(100);

    klog("B: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex);
    klog("B: acquired mutex\n");

    klog("B: shared memory contents: [%s]\n", shared_memory);
    klog("B: writing shm\n");
    strcpy(shared_memory, "Hi from B!");

    klog("B: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    klog("B: released mutex\n");
}

void process_c_main() {
    // sleep longer, to avoid race condition with other tasks
    sleep(100);

    klog("C: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex);
    klog("C: acquired mutex\n");

    klog("C: shared memory contents: [%s]\n", shared_memory);
    klog("C: writing shm\n");
    strcpy(shared_memory, "This is C calling");

    klog("C: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    klog("C: released mutex\n");
}
void process_d_main() {
    // sleep longer, to avoid race condition with other tasks
    sleep(100);

    klog("D: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex);
    klog("D: acquired mutex\n");

    klog("D: shared memory contents: [%s]\n", shared_memory);
    klog("D: writing shm\n");
    strcpy(shared_memory, "Dah Dah Dah");

    klog("D: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    klog("D: released mutex\n");
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
        default:
            klog("Received interrupt %d (0x%x), error %d\n", regs.int_no, regs.int_no, regs.err_code);
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(regs.int_no);
}
