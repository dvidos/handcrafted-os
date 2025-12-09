OUTPUT_FORMAT("elf32-i386")
ENTRY(kernel_main)

SECTIONS {
    . = 0x00100000;  /* match KERNEL_LOAD_ADDR in stage2.c, currently 1MB */

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }

    .stack_start = 0x90000; /* 576 KB */
    .stack_end   = .stack_start + 0x10000;  /* 64 KB */    
}
