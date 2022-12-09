#include "types.h"
#include "funcs.h"

// for BIOS interrupts information
// http://www.ctyme.com/intr/int.htm




char num_buff[16+1];


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

void bios_print_int(int num) {
    itoa(num, num_buff, 10);
    bios_print_str(num_buff);
}

void bios_print_hex(int num) {
    itoa(num, num_buff, 16);
    bios_print_str(num_buff);
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

int bios_detect_memory_map_e820(void *buffer, int *times) {
    // call INT 0x15, EAX=0xE820 in a loop.
    word error = 0;
    dword continuation = 0;

    // every pointed 24 bytes are filled as:
    // - 4 btyes base address low 32 bits
    // - 4 bytes base address high 32 bits
    // - 4 bytes length low 32 bit
    // - 4 bytes length high 32 bits
    // - 4 bytes type (and 4 bytes padding)
    *times = 0;
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
        (*times) += 1;
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

word bios_get_keystroke() {
    word output;
    asm(
        "mov $0x00, %%ah  \n\t"
        "int $0x16        \n\t"
        : "=a" (output) // ax -> output, al=ascii, ah=scan code
    );
}

