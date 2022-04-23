#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "screen.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "cpu.h"


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



/**
 * Things to implement from here:
 *
 * also can see chapters 14+ on https://github.com/cfenollosa/os-tutorial
 *
 * - better code structure & make file
 * - better screen handling (Cursor, scroll, etc)
 * - keyboard entry
 * - memory management (heap and -maybe- virtual memory)
 * - disk managmenet of some partition?
 * - multi processing and scheduler (+ timer)
 * - interrupt description table and handlers
 */


void kernel_main(void)
{
    disable_interrupts();  // interrupts are already disabled at this point


    screen_init();
    printf("C kernel running, kernel_main() is at 0x%p\n", (void *)kernel_main);

    // printf("sizeof(char)      is %d\n", sizeof(char)); // 2
    // printf("sizeof(short)     is %d\n", sizeof(short)); // 2
    // printf("sizeof(int)       is %d\n", sizeof(int));   // 4
    // printf("sizeof(long)      is %d\n", sizeof(long));  // 4 (was expecting 8!)
    // printf("sizeof(long long) is %d\n", sizeof(long long));  // 4 (was expecting 8!)
    // printf("17 in formats %%i [%i], %%x [%x], %%o [%o], %%b [%b]\n", 17, 17, 17, 17);
    // int snum = 3000000000;
    // unsigned int unum = 3000000000;
    // printf("signed int:   %%i [%i], %%u [%u], %%x [%x]\n", snum, snum, snum);
    // printf("unsigned int: %%i [%i], %%u [%u], %%x [%x]\n", unum, unum, unum);

    // screen_write("Initializing Programmable Interrupt Controller...");

    // init_pic();
    // screen_write(" done\n");
    //
    // // code segment selector: 0x08 (8)
    // // data segment selector: 0x10 (16)
    // screen_write("Initializing Global Descriptor Table...");
    // init_gdt();
    // screen_write(" done\n");
    //
    // screen_write("Initializing Interrupts Descriptor Table...");
    // init_idt(0x8);
    // screen_write(" done\n");
    // for(;;);

    // enable_interrupts();

    screen_write("Pausing forever...");
    for(;;)
        asm("hlt");
    screen_write("This should never appear on screen...");
}

void isr_handler(registers_t regs) {
    screen_write("interrupt! ");

    switch (regs.int_no) {
        case 0x20:
            // timer_handler(&regs);
            break;
        case 0x21:
            // keyboard_handler(&regs);
            break;
        default:
            // terminal_writestring("\nrecieved interrupt\taddress: ");
            // terminal_writehex32((uint32_t)&regs);
            // terminal_writestring("\tvalue: 0x");
            // terminal_writehex32(regs.int_no);
            // terminal_writestring("\terror: ");
            // terminal_writehex32(regs.err_code);
            // terminal_writestring("\n");
    }

    // PIC_sendEOI(regs.int_no);
}