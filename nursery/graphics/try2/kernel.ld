OUTPUT_FORMAT("elf32-i386")
ENTRY(kernel_main)

SECTIONS {
    . = 0x00100000;  /* match KERNEL_LOAD_ADDR, currently 1MB */

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
