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
    screen_init();
    screen_write("C kernel running\n");

    disable_interrupts();

    screen_write("Initializing Programmable Interrupt Controller...");
    init_pic();
    screen_write(" done!\n");

    // code segment selector: 0x08 (8)
    // data segment selector: 0x10 (16)
    screen_write("Initializing Global Descriptor Table...");
    init_gdt();
    screen_write(" done!\n");

    screen_write("Initializing Interrupts Descriptor Table...");
    init_idt(0x8);
    screen_write(" done!\n");

    enable_interrupts();

    screen_write("Another line!\n");
    screen_write("Another line 1!\n");
    screen_write("Another line 2!\n");
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
