[org 0x7C00]   
; boot sector is 512 bytes long,
; last two bytes need to have the values 0xAA55
; it is loaded by BIOS on memory address 0x7c00 to 0x7dff

; square brackets convert a value to a memory pointer
; move a number to al
;   mov al, 2
; move whatever is contained in this address (literal pointer)
;   mov al, [0x1234]
; move by pointer
;   mov bx, 0x1234
;   mov al, [bx]

; segments are there to allow us to address move then 64k of memory
; four segment registers: cs (code), ds (data), ss (stack), es (extra)
; each address is reference by the segment value, shifted left by a nibble (or multiplied by 16).
; this allows us to address the full... 640K of memory!!!
; e.g. if DS has 0x1000, then we can access addresses in the range of 0x10000 through 0x1FFFF
; in that case "mov ax, [0x20]" is actually pointing to 0x10020
; when we start up, all segment registers have the value 0x0000

; now, the "push" command diminishes the stack.
; the stack is pointed by sp. to set it up we can set the sp register
; choose an address well above, so pushing will not ruin our code
; any push operation will lower sp by two bytes and write the two bytes at sp, sp+1
; maybe that's why they call it 16 bits real mode.
; (fun detail, the address we initially set the SP to is never written)
; pusha pushes 16 bytes (8 registers i guess)
; in real mode, ss (the stack segment) can go only up to 0x9000, which allows addressing up to the 640kb we have

    mov bp, 0xffff  ; go to any address high enough, up to 0xffff actually!
    mov sp, bp

; call and ret are just pushing/popping the instruction pointer and jumping
; we pass arguments and get returned values in registers
; we use pusha and popa to preserve registers and avoid messing the caller's values
    
    mov bx, message  ; essentially move the address of the label
    call print_string
    call print_crlf

    mov bx, sp_now_is
    call print_string
    mov dx, sp
    call print_hex
    call print_crlf

    push 0x12

    mov bx, sp_now_is
    call print_string
    mov dx, sp
    call print_hex
    call print_crlf

    mov dx, cs
    call print_hex
    call print_crlf

    mov dx, ds
    call print_hex
    call print_crlf

    mov dx, ss
    call print_hex
    call print_crlf

    mov dx, es
    call print_hex
    call print_crlf




; Infinite loop (e9 fd ff)
    jmp $


; messages are zero terminated, to easily detect their end
message:
    db 'Loading...', 0
sp_now_is:
    db 'SP = ', 0

; function to print a string.
; string to be printed must be pointed by bx
; essentially we do "while (string[i] != 0) { print string[i]; i++ }"
print_string:
    pusha
print_byte:
    mov al, [bx]
    cmp al, 0    ; did we copy the zero terminator?
    je print_done

    mov ah, 0x0e ; bios print function, al has the byte
    int 0x10
    add bx, 1    ; have bx point to next byte
    jmp print_byte  ; next byte
print_done:
    popa
    ret


; function to print a new line.
; for some reason CR+LF is needed
print_crlf:
    pusha
    mov ah, 0x0e
    mov al, 0x0a
    int 0x10
    mov al, 0x0d
    int 0x10
    popa
    ret

    
; function to print a hex word, value in dx
; have a buffer available, then for the four nibbles do and rotate
; convert numbers to ascii by adding '0' (0x30). for values > 9, add 'A' (0x41)
print_hex:
    pusha
    mov cx, 0
print_hex_nibble:
    cmp cx, 4          ; if cx reached four, we are done
    je print_hex_done

    mov ax, dx
    and ax, 0x000f       ; keep last nibble
    add al, 0x30         ; add '0' to convert to ascii digit
    cmp al, 0x39         ; see if number or letter
    jle print_hex_skip_letter
    add al, 0x40 - 0x39  ; add extra value to go upto A-F range
print_hex_skip_letter:
    mov bx, print_hex_buffer + 5    ; point to last buffer byte
    sub bx, cx                      ; clever! subtract our index variable (cx)
    mov [bx], al                    ; copy al to where bx points now
    ; loop housekeeping
    ror dx, 4                       ; rotate dx by for, with rollover, to get to the next nibble
    add cx, 1
    jmp print_hex_nibble
print_hex_done:
    mov bx, print_hex_buffer  ; again, move the label address, not what is pointed by that address
    call print_string
    popa
    ret

print_hex_buffer:
    ; buffer for converting a word to hex string, to print it
    db '0x0000', 0



; Fill with 510 zeros minus the size of the previous code
times 510-($-$$) db 0

; Magic number
dw 0xaa55 
