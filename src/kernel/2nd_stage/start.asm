; assembly file used to enable jumping to specific address for the second stage boot loader
; purpose is to save the boot drive number, setup stack, then jump to the C main() function
[bits 16]

[EXTERN start]           ; this is declared in the C code


    ; just to see if this is working...
    mov ah, 0x0e ; bios print function, al has the byte
    mov al, ' '
    int 0x10
    mov al, '2'
    int 0x10
    mov al, 'n'
    int 0x10
    mov al, 'd'
    int 0x10

    ; print_string does not work, I suspect that the address is wrong...
    ; I think it relies on linker to re-address things...
    ; mov bx, second_stage_running
    ; call print_string
    
    ; ; stack top will be 0x0FFF0, 16 bytes below the address of the 2nd stage loader (0x10000)
    ; ; SS will be 0x00, SS will be 0xFFF0
    ; mov ax, 0x0
    ; mov ss, ax
    ; mov ax, 0xFFF0
    ; mov sp, ax

    ; ; now we can jump to our C version of boot loader
    ; mov ax, 0x1000     ; Code loaded at 64K
    ; mov cs, ax

    ; maybe clean some registers
    mov si, 0
    mov di, 0
    mov ah, 0  ; remember, al has the disk number
    mov bx, 0
    mov cx, 0
    mov dx, 0
    mov bp, 0

    jmp start

; second_stage_running:  db '2nd stage boot loader running', 0



; ; function to print a string.
; ; string to be printed must be pointed by bx
; ; essentially we do "while (string[i] != 0) { print string[i]; i++ }"
; print_string:
;     pusha
; print_byte:
;     mov al, [bx]
;     cmp al, 0    ; did we copy the zero terminator?
;     je print_done

;     mov ah, 0x0e ; bios print function, al has the byte
;     int 0x10
;     add bx, 1    ; have bx point to next byte
;     jmp print_byte  ; next byte
; print_done:
;     popa
;     ret

