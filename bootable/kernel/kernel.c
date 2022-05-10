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
#include "memory.h"
#include "string.h"
#include "multiboot.h"
#include "konsole.h"



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



void dump_multiboot_info(multiboot_info_t* mbi);
void dump_heap();

multiboot_info_t saved_multiboot_info;

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
    init_timer(1000);

    printf("Initializing Real Time Clock...\n");
    init_real_time_clock(15);

    // printf("Initializing memory...\n");
    // init_memory(boot_magic, mbi, (uint32_t)&kernel_start_address, (uint32_t)&kernel_end_address);

    size_t start = (size_t)&kernel_end_address;
    size_t end = 2 * 1024 * 1024;
    size_t num_pages = (end - start) / 4096;
    if (start >= end)
        panic("No good space for kernel pages");
    printf("Claiming kernel memory pages from %p to %p (%d pages)\n", start, end, num_pages);
    init_kernel_pages(start, end);
    

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
    }

    // printf("Starting konsole...\n");
    // konsole();

    printf("Testing task switching...\n");
    extern void test_switching_context_functionality();
    test_switching_context_functionality();
    extern void test_switching_start();
    test_switching_start();

    screen_write("Pausing forever...");
    for(;;)
        asm("hlt");
    screen_write("This should never appear on screen...");
}


void isr_handler(registers_t regs) {
    // don't forget we have mapped IRQs 0+ to 0x20+
    // to avoid the first 0x1F interrupts that are CPU faults in protected mode
    switch (regs.int_no) {
        case 0x20:
            timer_handler(&regs);
            extern void test_switching_tick();
            test_switching_tick();
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

    pic_send_eoi(regs.int_no);
}

// void sub2(int a, int b, int c) {
//     uint32_t a1 = 0x11111111;
//     uint32_t a2 = 0x22222222;
//     uint32_t esp = 0x33333333;
//     uint32_t a3 = 0x44444444;
//     register long esp1 asm ("esp");

//     asm("\t movl %%esp,%0" : "=r"(esp));

//     printf("Upon entry to sub2() ESP  is 0x%08x\n", esp);
//     printf("Upon entry to sub2() ESP1 is 0x%08x\n", esp1);

//     printf("Stack Dump (downwards)\n");
//     memdump((void *)((esp + 0x80) & ~0xF), 16 * 10, true);
// }
// void sub1(int magic) {
//     uint32_t a = 0x55555555;
//     uint32_t esp = 0x66666666;
//     uint32_t d = 0x77777777;

//     // 0x10BFB8, subcalling gives smaller SP values
//     asm("\t movl %%esp,%0" : "=r"(esp));

//     printf("sub1() address is 0x%08p\n", sub1);
//     printf("sub2() address is 0x%08p\n", sub2);
//     printf("Upon entry to sub1() ESP is 0x%08x\n", esp);

//     sub2(0x99AABBCC, 0x55667788, 0x11223344);
// }
