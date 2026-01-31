; start.asm — kernel entry point
; NASM syntax, Intel flavor

[BITS 32]           ; assuming we switch to protected mode
[SECTION .text.start]
global _start

_start:
    cli             ; disable interrupts
    ; optionally set up stack if stage2 didn't
    ; mov esp, KERNEL_STACK_TOP
    ; clear BSS or jump to main kernel init
    ; display boot banner for now
    mov eax, 0xb8000        ; VGA text buffer
    mov byte [eax], 'K'
hang:
    hlt
    jmp hang
