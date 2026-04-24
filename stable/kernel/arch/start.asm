; start.asm — kernel entry point
; NASM syntax, Intel flavor

[BITS 32]           ; assuming we switch to protected mode
[SECTION .text.start]
global _start
extern kernel_main

_start:
    cli             ; disable interrupts

    ; setup stack, must be 16 bytes aligned
    mov esp, KERNEL_STACK_TOP

    mov ebp, 0      ; signify no previous frame
    push ebp        ; push a dummy return address (optional)
    push ebp        ; push a dummy "previous EBP"

    ; EAX contains pointer to boot_info
    push eax
    call kernel_main
    
hang:
    hlt
    jmp hang
