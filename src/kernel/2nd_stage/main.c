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



unsigned char boot_drive; // passed to us by first boot loader, from BIOS


struct mem_map_entry_24 {
    uint32 address_low;
    uint32 address_high;
    uint32 size_low;
    uint32 size_high;
    uint32 type;
    uint32 padding;
};

void _print_mem_map_entry(struct mem_map_entry_24 *entry);

void start_c() {
    char *buffer;
    
    // save the boot drive to a variable
    __asm__("mov %%dl, %0" : "=g" (boot_drive));

    // we have 32k from 0x7E00 up to stack at 0xFFFF
    buffer = (char *)0x8000;

    bios_print_str("Second stage boot loader running...\r\n");

    bios_print_str("Boot drive: 0x");
    bios_print_hex(boot_drive);
    bios_print_str("\r\n");
    
    word low_mem_kb = bios_get_low_memory_in_kb();
    bios_print_str("Low memory value: ");
    bios_print_int(low_mem_kb);
    bios_print_str(" KB\r\n");
    
    bios_print_str("Clearing buffer...");
    memset(buffer, 0, 512);
    bios_print_str("ok\r\n");

    bios_print_str("Reading memory map...");
    int times = 0;
    int err = bios_detect_memory_map_e820(buffer, &times);
    if (err) {
        bios_print_str("Error getting memory map");
    } else {
        bios_print_str("Memory map follows:\r\n");
        struct mem_map_entry_24 *mmp = (struct mem_map_entry_24 *)buffer;
        for (int i = 0; i < 8; i++) {
            _print_mem_map_entry(mmp);
            mmp++;
        }
        bios_print_str("(end of map)\r\n");
    }

    // query graphics modes
    // select graphics mode, find way to work even through protected mode.

    // gather memory map

    // gather multiboot2 information

    // navigate file system, to find /boot/kernel or something.

    // parse kernel ELF header, load kernel into memory.

    // setup mini gdt, enter protected mode

    // jump to kernel, pass magic number and multiboot information
    bios_print_str("Nothing else to do, freezing for now..."); 
    freeze();
}





void _print_mem_map_entry(struct mem_map_entry_24 *entry) {
    bios_print_str("  Addr 0x");
    bios_print_hex(entry->address_high);
    bios_print_str(":");
    bios_print_hex(entry->address_low);
    bios_print_str("  ");

    bios_print_str("Size 0x");
    bios_print_hex(entry->size_high);
    bios_print_str(":");
    bios_print_hex(entry->size_low);
    bios_print_str("  ");

    bios_print_str("Type ");
    bios_print_int(entry->type);
    bios_print_str("\r\n");
}



void *heap_start;
void *heap_end;
void *heap_next_free_block;

void init_heap(word start, word end) {
    // casting to different size
    heap_start = (void *)(int)start;
    heap_end = (void *)(int)end;

    memset(heap_start, 0, heap_end - heap_start);
    heap_next_free_block = heap_start;
}

void *malloc(int size) {
    if (heap_next_free_block + size > heap_end) {
        bios_print_str("Panic: heap space exhausted");
        freeze();
    }
    void *ptr = heap_next_free_block;
    heap_next_free_block += size;
    return ptr;
}

word heap_free_space() {
    return (heap_end - heap_next_free_block);
}

