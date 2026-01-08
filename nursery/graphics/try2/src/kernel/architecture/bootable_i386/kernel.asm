[BITS 32]

global _kernel_start
extern kernel_main

; ensure this method is first in the binary
_kernel_start:
    cli                 ; just in case
    mov ebp, 0          ; to signify no previous frame
    call kernel_main
.hang:
    hlt
    jmp .hang

; ----------------------------------------------------

global lidt_asm
lidt_asm:
    mov eax, [esp+4]  ; get the pointer argument from the stack
    lidt [eax]         ; load the IDT from memory
    ret

; ----------------------------------------------------

global exception_stub_asm
extern cpu_exception
exception_stub_asm:
    pusha
    call cpu_exception
    popa
    iretd

; ----------------------------------------------------

%macro IRQ_STUB 2
    global %1
    extern %2
%1:
    pusha
    mov ax, 0x10        ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call %2
    popa
    iretd
%endmacro

IRQ_STUB irq0_stub_asm,  timer_isr
IRQ_STUB irq1_stub_asm,  keyboard_isr
IRQ_STUB irq12_stub_asm, mouse_isr

