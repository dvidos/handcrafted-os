/*
    running in real mode,
    purpose is to take advantage of real mode Interrups,
    collect information
    possibly enter graphics mode
    find, load and execute final kernel

    this second stage boot loader will be loaded in 0x10000 (i.e. 64K)
*/

#define NULL   0

void print_string(char *s);

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
