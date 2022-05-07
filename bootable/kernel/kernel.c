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
    cli();  // interrupts are already disabled at this point

    screen_init();
    printf("C kernel running, loaded at 0x%x - 0x%x (size of %u KB)\n",
        (uint32_t)&kernel_start_address,
        (uint32_t)&kernel_end_address,
        ((uint32_t)&kernel_end_address - (uint32_t)&kernel_start_address) / 1024
    );

    if (boot_magic == 0x2BADB002) {
        printf("Bootloader info detected\n");
        memcpy((char *)&saved_multiboot_info, (char *)mbi, sizeof(multiboot_info_t));
    } else {
        printf("No bootloader info detected\n");
        memset((char *)&saved_multiboot_info, 0, sizeof(multiboot_info_t));
    }

    // uint32_t r = get_cpuid_availability();
    // printf("get_cpuid_availability() returned 0x%08x\n", r);

    // printf("sizeof(char)      is %d\n", sizeof(char)); // 2
    // printf("sizeof(short)     is %d\n", sizeof(short)); // 2
    // printf("sizeof(int)       is %d\n", sizeof(int));   // 4
    // printf("sizeof(long)      is %d\n", sizeof(long));  // 4 (was expecting 8!)
    // printf("sizeof(long long) is %d\n", sizeof(long long)); // 8
    // printf("sizeof(void *) is %d\n", sizeof(void *));  // 4
    // printf("17 in formats %%i [%i], %%x [%x], %%o [%o], %%b [%b]\n", 17, 17, 17, 17);
    // int snum = 3000000000;
    // unsigned int unum = 3000000000;
    // printf("signed int:   %%i [%i], %%u [%u], %%x [%x]\n", snum, snum, snum);
    // printf("unsigned int: %%i [%i], %%u [%u], %%x [%x]\n", unum, unum, unum);


    // code segment selector: 0x08 (8)
    // data segment selector: 0x10 (16)
    screen_write("Initializing Global Descriptor Table...");
    init_gdt();
    screen_write(" done\n");

    screen_write("Initializing Interrupts Descriptor Table...");
    init_idt(0x8);
    screen_write(" done\n");

    screen_write("Initializing Programmable Interrupt Controller...");
    init_pic();
    screen_write(" done\n");

    screen_write("Initializing Programmable Interval Timer...");
    init_timer(1000);
    screen_write(" done\n");

    screen_write("Initializing memory...");
    init_memory(boot_magic, mbi, (uint32_t)&kernel_start_address, (uint32_t)&kernel_end_address);
    screen_write(" done\n");

    sti();

    if (strcmp((char *)saved_multiboot_info.cmdline, "tests") == 0) {
        printf("Running tests...\n");
        extern void run_tests();
        run_tests();
        screen_write("Tests finished, pausing forever...");
        for(;;)
            asm("hlt");
    }

    printf("Starting konsole...\n");
    konsole();

    screen_write("Pausing forever...");
    for(;;)
        asm("hlt");
    screen_write("This should never appear on screen...");
}

void isr_handler(registers_t regs) {
    switch (regs.int_no) {
        case 0x20:
            timer_handler(&regs);
            break;
        case 0x21:
            keyboard_handler(&regs);
            break;
        default:
            printf("Received interrupt %d, error %d\n", regs.int_no, regs.err_code);
    }

    pic_send_eoi(regs.int_no);
}
