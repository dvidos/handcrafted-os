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

void print_string(char *s);


u16 bios_screen_functions(u16 ax) {
    u16 status;
    asm(
        "movl %1, %%ax \n\t"
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
        "movl %1, %%ax \n\t"
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
        "movl %1, %%ax \n\t"
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
        "movl %1, %%ax \n\t"
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
    /* Do something with arg, perhaps a syscall,
       or inline assembly? */
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

void main() {
    print_string("Second stage loader running...");

    loop_something(2, 5);
    do_something(6);
    loop_something(5, 2);
    do_something(1);

    // gather memory map

    // gather multiboot2 information

    // load final kernel

    // setup mini gdt, enter protected mode

    // jump to kernel, pass magic number and multiboot information
}

void print_string(char *s) {
    // print string by calling BIOS interrupts
    s = NULL;
}

void load_disk_sector(char *s) {
    // print string by calling BIOS interrupts
}
