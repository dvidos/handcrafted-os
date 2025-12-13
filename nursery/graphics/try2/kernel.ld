OUTPUT_FORMAT("elf32-i386")
ENTRY(kernel_main)

SECTIONS {
    . = KERNEL_LOAD_ADDRESS;

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
