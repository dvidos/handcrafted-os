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
#include "multiboot.h"


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
uint32_t __kernel_start_address;
uint32_t __kernel_end_address;

void dump_boot_info(multiboot_info_t* mbd);


// arguments from the multiboot loader, normally left by GRUB
// see https://wiki.osdev.org/Detecting_Memory_(x86)
void kernel_main(multiboot_info_t* mbd, unsigned int magic)
{
    disable_interrupts();  // interrupts are already disabled at this point

    screen_init();
    printf("C kernel running\n");

    if (magic == 0x2BADB002) {
        printf("Bootloader info detected\n", magic);
        dump_boot_info(mbd);
    }


    printf("Kernel start at 0x%x (decimal %u)\n", (uint32_t)&__kernel_start_address, (uint32_t)&__kernel_start_address);
    printf("Kernel end   at 0x%x (decimal %u)\n", (uint32_t)&__kernel_end_address, (uint32_t)&__kernel_end_address);
    size_t kernel_size = (size_t)(&__kernel_end_address) - (size_t)(&__kernel_start_address);
    printf("Kernel size %d bytes\n", kernel_size);

    // uint32_t r = get_cpuid_availability();
    // printf("get_cpuid_availability() returned 0x%08x\n", r);

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
    init_memory(((void *)&__kernel_end_address) + 1024);
    screen_write(" done\n");

    enable_interrupts();





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

void dump_boot_info(multiboot_info_t* mbd) {
    printf("Multiboot information\n");
    printf("- flags:       0x%08x\n", mbd->flags);
    if (mbd->flags & MULTIBOOT_INFO_MEMORY) {
        printf("- mem lower:   %u KB\n", mbd->mem_lower);
        printf("- mem upper:   %u KB\n", mbd->mem_upper);
    }
    if (mbd->flags & MULTIBOOT_INFO_BOOTDEV) {
        printf("- boot device: 0x%x\n", mbd->boot_device);
    }
    if (mbd->flags & MULTIBOOT_INFO_CMDLINE) {
        printf("- cmd line:    \"%s\" (at 0x%08x)\n", mbd->cmdline, mbd->cmdline);
    }
    if (mbd->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
        printf("- boot loader: \"%s\" (at 0x%08x)\n", mbd->boot_loader_name, mbd->boot_loader_name);
    }
    if (mbd->flags & MULTIBOOT_INFO_MEM_MAP) {
        printf("- mmap length: %u\n", mbd->mmap_length);
        printf("- mmap addr:   0x%08x\n", mbd->mmap_addr);

        uint64_t total_available = 0;
        int size = mbd->mmap_length;
        multiboot_memory_map_t *entry = (multiboot_memory_map_t *)mbd->mmap_addr;
        printf("    Address              Length               Type\n");
        // ntf("  0x12345678:12345678  0x12345678:12345678  0x12345678")
        while (size > 0) {
            char *type;
            switch (entry->type) {
                case MULTIBOOT_MEMORY_AVAILABLE:
                    type = "Available";
                    break;
                case MULTIBOOT_MEMORY_RESERVED:
                    type = "Reserved";
                    break;
                case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                    type = "Reclaimable";
                    break;
                case MULTIBOOT_MEMORY_NVS:
                    type = "NVS";
                    break;
                case MULTIBOOT_MEMORY_BADRAM:
                    type = "Bad RAM";
                    break;
                default:
                    type = "Unknown";
                    break;
            }
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                printf("    0x%08x:%08x  0x%08x:%08x  %s\n",
                    (uint32_t)(entry->addr >> 32),
                    (uint32_t)(entry->addr & 0xFFFFFFFF),
                    (uint32_t)(entry->len >> 32),
                    (uint32_t)(entry->len & 0xFFFFFFFF),
                    type
                );
                total_available += entry->len;
            }
            size -= sizeof(multiboot_memory_map_t);
            entry++;
        }
        printf("    Total %u MB available memory\n", (uint32_t)(total_available / (1024 * 1024)));

    }
}
