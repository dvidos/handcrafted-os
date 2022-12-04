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




#define NULL   0
typedef unsigned short u16;

void bios_print_char(unsigned char c);
void bios_print_str(unsigned char *str);

unsigned char boot_drive;

void start() {
    // save the boot drive to a variable

    __asm__("mov $0x0e, %ah");  // bios print function, al has the byte
    __asm__("mov ' ', %al");
    __asm__("int $0x10");
    __asm__("mov 'w', %al");
    __asm__("int $0x10");
    __asm__("mov 'o', %al");
    __asm__("int $0x10");
    __asm__("mov 'w', %al");
    __asm__("int $0x10");


    asm("nop");
    asm("nop");
    asm("nop");

    __asm__("mov %%al, %0" : "=g" (boot_drive));

    asm("nop");
    asm("nop");
    asm("nop");
    bios_print_char('h');
    bios_print_char('e');
    bios_print_char('l');
    bios_print_char('l');
    bios_print_char('o');
    bios_print_char('!');
    asm("nop");
    asm("nop");
    asm("nop");

    // maybe it's the data segment who is to blame...
    /*
        So the problem is that the code thinks it's loaded at zero address
        and all variables are relative to zero,
        while we do load it at 0x10000. 
        Maybe setting DS to 0x1000 would help?

        100e7:       90                      nop
        100e8:       90                      nop
        100e9:       90                      nop
        100ea:       66 83 ec 0c             sub    esp,0xc
        100ee:       66 68 5c 01 00 00       pushd  0x15c     ; offset of string, zero based.
        100f4:       66 e8 26 00 00 00       calld  0x10120   ; the print_str() function.
        100fa:       66 83 c4 10             add    esp,0x10
        100fe:       f4                      hlt    
        100ff:       eb fd                   jmp    0x100fe

        00000150  00 84 c0 75 d2 90 90 66  c9 66 c3 90 48 65 6c 6c  |...u...f.f..Hell|
        00000160  6f 20 66 72 6f 6d 20 61  20 43 20 73 74 72 69 6e  |o from a C strin|
        00000170  67 2c 20 77 65 27 6c 6c  20 73 65 65 20 77 68 61  |g, we'll see wha|
        00000180  74 20 77 65 20 63 61 6e  20 64 6f 2e 00 00 00 00  |t we can do.....|
        00000190  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|

    */

    bios_print_str("Hello from a C string, we'll see what we can do.");
    // print_string("Second stage loader running...");

    // loop_something(2, 5);
    // do_something(6);
    // loop_something(5, 2);
    // do_something(1);

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