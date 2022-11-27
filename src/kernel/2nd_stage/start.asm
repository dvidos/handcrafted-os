; assembly file used to enable jumping to specific address for the second stage boot loader
; purpose is to save the boot drive number, setup stack, then jump to the C main() function
[bits 16]


[EXTERN main]           ; this is declared in the C code


; [global _start]   
; _start:
    ; just to make sure. 
    cli
    
    ; stack top will be 0x0FFF0, 16 bytes below the address of the 2nd stage loader (0x10000)
    ; SS will be 0x00, SS will be 0xFFF0
    mov ax, 0x0
    mov ss, ax
    mov ax, 0xFFF0
    mov sp, ax

    ; now we can jump to our C version of boot loader
    mov ax, 0x1000     ; Code loaded at 64K
    mov cs, ax

    call main
    jmp $                   ; freeze, main() is not supposed to return...


