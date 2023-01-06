; assembly file used to enable jumping to specific address for the second stage boot loader
; purpose is to save the boot drive number, setup stack, then jump to the C main() function
[bits 16]

[extern start_c]           ; this is declared in the C code
[global start_asm]

start_asm:
    ; just to see if this is working...
    ; mov ah, 0x0e ; bios print function, al has the byte
    ; mov al, ' '
    ; int 0x10
    ; mov al, '2'
    ; int 0x10
    ; mov al, 'n'
    ; int 0x10
    ; mov al, 'd'
    ; int 0x10

    ; mov bx, second_stage_running
    ; call print_string
    
    ; stack top will be 0x0FFF0, 16 bytes below the top address of this segment
    mov ax, 0xFFF0
    mov sp, ax

    ; maybe clean some registers (DL should have the boot drive no)
    mov ax, 0
    mov bx, 0
    mov cx, 0
    mov dh, 0 ; note, maintain DL
    mov bp, 0
    mov si, 0
    mov di, 0

    jmp start_c

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

