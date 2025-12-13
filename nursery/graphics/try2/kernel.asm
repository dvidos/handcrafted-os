[BITS 32]

global _kernel_start
extern kernel_main

; ensure this method is first in the binary
_kernel_start:
    mov ebp, 0
    call kernel_main
.hang:
    hlt
    jmp .hang