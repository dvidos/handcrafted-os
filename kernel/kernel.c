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
#include "kheap.h"
#include "string.h"
#include "multiboot.h"
#include "konsole.h"
#include "process.h"



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
void process_d_main();


// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(multiboot_info_t* mbi, unsigned int boot_magic)
{
    screen_init();
    printf("C kernel running, loaded at 0x%x - 0x%x (size of %u KB)\n",
        (uint32_t)&kernel_start_address,
        (uint32_t)&kernel_end_address,
        ((uint32_t)&kernel_end_address - (uint32_t)&kernel_start_address) / 1024
    );

    // it seems interrupts are disabled, nmi is also disabled
    // printf("Interrupts enabled? %d\n", interrupts_enabled());
    // printf("NMI is enabled? %d\n", nmi_enabled());

    if (boot_magic == 0x2BADB002) {
        printf("Bootloader info detected\n");
        memcpy((char *)&saved_multiboot_info, (char *)mbi, sizeof(multiboot_info_t));
    } else {
        printf("No bootloader info detected\n");
        memset((char *)&saved_multiboot_info, 0, sizeof(multiboot_info_t));
    }

    // code segment selector: 0x08 (8)
    // data segment selector: 0x10 (16)
    printf("Initializing Global Descriptor Table...\n");
    init_gdt();

    printf("Initializing Interrupts Descriptor Table...\n");
    init_idt(0x8);

    printf("Initializing Programmable Interrupt Controller...\n");
    init_pic();

    printf("Initializing Programmable Interval Timer...\n");
    init_timer();

    printf("Initializing Real Time Clock...\n");
    init_real_time_clock(15);

    printf("Initializing Serial Port 1 for logging...\n");
    init_serial_port();
    serial_write("Hello from kernel\n");
    
    printf("Initializing Kernel Heap...\n");
    // we will reserve from kernel code end, till 2 MB, for kernel heap
    init_kernel_heap(((char *)&kernel_end_address) + 128, (char *)0x1FFFFF);

    printf("Enabling interrupts...\n");
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

    init_multitasking();

    // create desired tasks here, 
    start_process(create_process(process_a_main, "Task A"));
    start_process(create_process(process_b_main, "Task B"));
    start_process(create_process(process_c_main, "Task C"));
    start_process(create_process(process_d_main, "Task D"));

    // start_multitasking() will never return
    start_multitasking();


    printf("Freeze");
    for(;;)
        asm("hlt");
    screen_write("This should never appear on screen...");
}

mutex_t shared_memory_mutex;
char shared_memory[256];

void process_a_main() {

    memset(&shared_memory_mutex, 0, sizeof(shared_memory_mutex));
    shared_memory_mutex.limit = 1;

    printf("A: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex); // this will block
    printf("A: writing shared memory\n");
    memset(shared_memory, 0, sizeof(shared_memory));
    strcpy(shared_memory, "Hi there from task a!\n");
    sleep_me_for(100);
    printf("A: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    yield();
    printf("A: re-acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex); // this will block
    printf("A: reading shared memory\n");
    printf("> %s\n", shared_memory);
    sleep_me_for(100);
    printf("A: done, releasing mutex\n");
    release_mutex(&shared_memory_mutex);


    // int i = 1;
    // while (true) {
    //     printf("This is A, i=%d\n", i++);
    //     sleep_me_for(200);
    //     if (i > 10)
    //         break;
    // }
}

void process_b_main() {

    yield();
    printf("B: acquiring mutex\n");
    acquire_mutex(&shared_memory_mutex); // this will block
    printf("B: reading shared memory\n");
    printf("> %s\n", shared_memory);
    memset(shared_memory, 0, sizeof(shared_memory));
    strcpy(shared_memory, "Task B listening loud and clear!\n");
    sleep_me_for(100);
    printf("B: releasing mutex\n");
    release_mutex(&shared_memory_mutex);
    yield();


    // for (int i = 0; i < 10; i++) {
    //     printf("This is B, i is %d\n", i++);
    //     sleep_me_for(300);
    // }
}
void process_c_main() {
    for (int i = 0; i < 10; i++) {
        // printf("Task C here, this is the %d time\n", i);
        sleep_me_for(1000);
    }
}
void process_d_main() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            // printf("%d + %d = %d\n", i, j, i + j);
            // sleep_me_for(100);
        }
    }
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            // printf("%d * %d = %d\n", i, j, i * j);
            // sleep_me_for(100);
        }
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
        default:
            printf("Received interrupt %d (0x%x), error %d\n", regs.int_no, regs.int_no, regs.err_code);
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(regs.int_no);
}
