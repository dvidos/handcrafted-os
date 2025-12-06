ENTRY(kmain)

SECTIONS
{
    . = 0x400; /* Start at 1 KiB so header is near start */

    .multiboot : {
        KEEP(*(.multiboot))
    }

    .text : { *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data) }
    .bss  : { *(.bss) *(COMMON) }
}
