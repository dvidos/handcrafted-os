/*
    running in real mode,
    purpose is to take advantage of real mode Interrups,
    collect information
    possibly enter graphics mode
    find, load and execute final kernel

    this second stage boot loader will be loaded in 0x10000 (i.e. 64K)

    maybe this will help: https://dc0d32.blogspot.com/2010/06/real-mode-in-c-with-gcc-writing.html
    or this: http://www.cs.cmu.edu/~410-s07/p4/p4-boot.pdf
    definitely these
*/
#include "types.h"
#include "funcs.h"
#include "bios.h"
#include "memory.h"
#include "disk.h"




void start_c() {
    byte boot_drive; // passed to us by first boot loader, from BIOS
    
    // save the boot drive to a variable
    __asm__("mov %%dl, %0" : "=g" (boot_drive));

    printf("\nSecond stage boot loader running...\n");
    printf("Boot drive: 0x%x\n", boot_drive);

    // we are loaded at 0x1000, while top of stack is at 0xFFF0
    // code and data sections may be about 0x4000 in total (16 KB)
    // allowing 0x1000 (4 KB) for stack, at have 40 KB for data, at 0x5000
    init_heap(0x5000, 0xF000);
    printf("Heap initialized, %d KB space available\n", heap_free_space() / 1024);


    // gather memory map
    detect_memory();


    // query graphics modes
    // select graphics mode, find way to work even through protected mode.


    // gather multiboot2 information

    // navigate file system, to find /boot/kernel or something.
    // parse kernel ELF header, load kernel into memory.
    find_and_load_kernel(boot_drive, 0x1000);

    // setup mini gdt, enter protected mode

    // move kernel from real-mode segment to 0x100000 (1MB)

    
    // jump to kernel, pass magic number and multiboot information


    bios_print_str("Nothing else to do, freezing for now..."); 
    freeze();
}
