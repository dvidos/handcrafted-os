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



void bios_print_char(unsigned char c);
void bios_print_str(unsigned char *str);
word bios_get_low_memory_in_kb();
int bios_detect_memory_map();

unsigned char boot_drive; // passed to us by first boot loader, from BIOS
char buffer[512];
char numbuff[32];

struct mem_map_entry_24 {
    uint32 address_high;
    uint32 address_low;
    uint32 size_high;
    uint32 size_low;
    uint32 type;
    uint32 padding;
};


void start() {
    // save the boot drive to a variable
    __asm__("mov %%al, %0" : "=g" (boot_drive));

    bios_print_str("\r\nSecond stage boot loader running...\r\n");

    word low_mem_kb = bios_get_low_memory_in_kb();
    itoa(low_mem_kb, numbuff, 10);
    bios_print_str("Low memory value: ");
    bios_print_str(numbuff);
    bios_print_str(" KB\r\n");
    

    memset(buffer, 0, sizeof(buffer));
    int err = bios_detect_memory_map(buffer);
    if (err) {
        bios_print_str("Error getting memory map\r\n");
    } else {
        bios_print_str("Memory map follows:\r\n");
        struct mem_map_entry_24 *mmp = (struct mem_map_entry_24 *)buffer;
        for (int i = 0; i < 8; i++) {

            // memset(numbuff, 0, sizeof(numbuff));
            // itoa(mmp->address_high, numbuff, 16);
            // bios_print_str("Addr 0x");
            // bios_print_str(numbuff);
            // bios_print_str(":");

            // memset(numbuff, 0, sizeof(numbuff));
            // itoa(mmp->address_low, numbuff, 16);
            // bios_print_str("0x");
            // bios_print_str(numbuff);

            // bios_print_str("  ");


            // memset(numbuff, 0, sizeof(numbuff));
            // itoa(mmp->size_high, numbuff, 16);
            // bios_print_str("Size 0x");
            // bios_print_str(numbuff);
            // bios_print_str(":");

            // memset(numbuff, 0, sizeof(numbuff));
            // itoa(mmp->size_low, numbuff, 16);
            // bios_print_str("0x");
            // bios_print_str(numbuff);

            // bios_print_str("  ");


            memset(numbuff, 0, sizeof(numbuff));
            itoa(mmp->type, numbuff, 16);
            bios_print_str("Type 0x");
            bios_print_str(numbuff);

            bios_print_str("\r\n");
            mmp += 1;
        }
        bios_print_str("Done\r\n");
    }

    // query graphics modes
    // select graphics mode, find way to work even through protected mode.


    // gather memory map

    // gather multiboot2 information

    // navigate file system, to find /boot/kernel or something.

    // parse kernel ELF header, load kernel into memory.

    // setup mini gdt, enter protected mode

    // jump to kernel, pass magic number and multiboot information
    for (;;) asm("hlt");
}



void bios_print_char(unsigned char c) {
    // AH shall have 0x0e (print char function)
    // AL will have the byte to print
    __asm__(
        "mov $0x0e, %%ah \n\t"
        "mov %0, %%al \n\t"
        "int $0x10 \n\t"
        : // outputs
        : "g" (c) // inputs
        : // clobbers
    );
}

void bios_print_str(unsigned char *str) {
    while (*str) {
        bios_print_char(*str);
        str++;
    }
}

word bios_get_low_memory_in_kb() {
    // detecting low memory, INT 12
    word value = 0;
    word error = 0;

    asm(
        "clc            \n\t"  // clear carry
        "mov $0, %%dx \n\t"
        "int $0x12      \n\t"
        "jnc 1f    \n\t"  // carry will be set on error
        "mov $1, %%dx     \n\t"  // set error flag
        "1:       \n\t"
        : "=d" (error), 
          "=a" (value)  // outputs
        : // inputs
         // clobbered
    );

    return error ? 0 : value;
}

int bios_detect_memory_map(void *buffer) {
    // call INT 0x15, EAX=0xE820 in a loop.
    word error = 0;
    dword continuation = 0;

    // every pointed 24 bytes are filled as:
    // - 4 btyes base address low 32 bits
    // - 4 bytes base address high 32 bits
    // - 4 bytes length low 32 bit
    // - 4 bytes length high 32 bits
    // - 4 bytes type
    while (true) {
        asm(
            "mov $0xE820, %%eax   \n\t"
            "mov %2, %%di   \n\t"   // pointer to data area (ES:DI actually)
            "mov %3, %%ebx   \n\t"   // continuation value form prev call
            "mov $24, %%ecx \n\t"  // buffer size
            "mov $0x534D4150, %%edx \n\t" // 'SMAP' magic value
            "int $0x15      \n\t"

            // check success
            "cmp $0x534D4150, %%eax \n\t"
            "jne 1f \n\t"  // jump (in not equal) to the next "1" forward label
            "jc 1f \n\t"  // if carry is set, it's an error
            "jmp 2f \n\t"
            "1: \n\t"
            "movw $1, %0 \n\t"   // error flag

            // if we are here, success
            "2:  \n\t"
            "mov %%ebx, %1 \n\t"  // continuation value

            // outputs
            : "=g" (error), 
            "=g" (continuation)
            // inputs
            : "g" (buffer),
            "g" (continuation)

            // clobbered
            : "%eax", "%ebx", "%ecx", "%edx", "%edi"
        );

        if (error || continuation == 0)
            break;
        buffer += 24;
    }

    return error ? ERROR : SUCCESS;
}


/*
u16 bios_screen_functions(u16 ax) {
    u16 status;
    asm(
        "movw %1, %%ax \n\t"
        "int $0x10 \n\t"
        "movw %%ax, %0 \n\t"
        : "=g" (status)
        : "g" (ax)
        : "%ax" 
    );
    return status;
}

u16 bios_keyboard_functions(u16 ax) {
    u16 status;
    asm(
        "movw %1, %%ax \n\t"
        "int $0x16 \n\t"
        "movw %%ax, %0 \n\t"
        : "=g" (status)
        : "g" (ax)
        : "%ax" 
    );
    return status;
}

u16 bios_memory_functions(u16 ax) {
    u16 status;
    asm(
        "movw %1, %%ax \n\t"
        "int $0x15 \n\t"
        "movw %%ax, %0 \n\t"
        : "=g" (status)
        : "g" (ax)
        : "%ax" 
    );
    return status;
}
u16 bios_disk_functions(u16 ax) {
    u16 status;
    asm(
        "movw %1, %%ax \n\t"
        "int $0x13 \n\t"
        "movw %%ax, %0 \n\t"
        : "=g" (status)
        : "g" (ax)
        : "%ax" 
    );
    return status;
}

void do_something(int arg)
{
    // Do something with arg, perhaps a syscall, or inline assembly?
}

void loop_something(int from, int to)
{
    int  arg;

    if (from <= to)
        for (arg = from; arg <= to; arg++)
            do_something(arg);
    else
        for (arg = from; arg <= to; arg--)
            do_something(arg);
}

void print_string(char *s) {
    // print string by calling BIOS interrupts
    s = NULL;
}

void load_disk_sector(char *s) {
    // print string by calling BIOS interrupts
}
*/

