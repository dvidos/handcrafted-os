; start.asm — kernel entry point
; NASM syntax, Intel flavor

[BITS 32]           ; assuming we switch to protected mode
[SECTION .text.start]
global _start
extern kernel_main

_start:
    cli             ; disable interrupts

    ; optionally set up stack if stage2 didn't
    mov esp, KERNEL_STACK_TOP

    mov ebp, 0          ; to signify no previous frame

    call kernel_main
hang:
    hlt
    jmp hang
