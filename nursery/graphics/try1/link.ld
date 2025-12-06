ENTRY(_start)

SECTIONS
{
    . = 0x100;

    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    .text   ALIGN(4) : { *(.text) }
    .rodata ALIGN(4) : { *(.rodata*) }
    .data   ALIGN(4) : { *(.data) }
    .bss    ALIGN(4) : { *(.bss) *(COMMON) }
    .stack (NOLOAD) : {
        stack_start = .;
        stack_top   = . + 16K;
        . = stack_top;
    }
}
