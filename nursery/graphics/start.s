    .section .text
    .global _start
    .type _start, @function

_start:
    cli                 # disable interrupts

    push %ebx           # multiboot info pointer (2nd param)
    push %eax           # magic number (1st param)
    call kmain          # call your C kernel entry

1:
    hlt
    jmp 1b              # halt forever
