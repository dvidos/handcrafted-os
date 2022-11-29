; boot sector is 512 bytes long,
; last two bytes need to have the values 0xAA55 for it to detect a bootable sector
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

; call and ret are just pushing/popping the instruction pointer and jumping
; we pass arguments and get returned values in registers
; we use pusha and popa to preserve registers and avoid messing the caller's values
; in real mode, ss (the stack segment) can go only up to 0x9000, which allows addressing up to the 640kb we have


[org 0x7C00]
[bits 16]

    ; kernel will enable them at a later time
    cli

    ; save the drive number that bios loaded us from
    ; needed when loading the kernel from subsequent sectors
    mov [boot_drive_no], dl

    ; we are loaded at 0x7C00 through 0x7E00
    mov ax, 0xFFF0  ; go to any address high enough, up to 0xffff actually!
    mov sp, ax
    
    mov bx, boot_sector_running  ; essentially move the address of the label
    call print_string
    call print_crlf

    mov bx, loading_2nd_stage    ; this is just too much text!
    call print_string
    call print_crlf

    ; load the second stage boot loader
    mov bx, 0x2000           ; where to load the 2nd stage (0x2000 = 8K)
    mov dh, 4                ; how many sectors to load (see linker script for 2nd stage boot loader)
    mov dl, [boot_drive_no]  ; from the same drive where the boot sector was
    call load_sectors

    mov bx, executing_2nd_stage
    call print_string

    ; our segments have the value of zero,
    ; our SP points to something like 0xFFF0 (e.g. high enough)
    ; our code is loaded at 0x2000
    ; let's give it the drive number too.
    mov dl, [boot_drive_no] 

    ; here goes nothing!
    jmp 0x2000



boot_drive_no:
    db 0 ; we will store the boot drive here

; messages are zero terminated, to easily detect their end
boot_sector_running:  db 'Boot sector running', 0
loading_2nd_stage:    db 'Loading 2nd stage...', 0
executing_2nd_stage:  db 'Executing 2nd stage...', 0



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
; for some reason CR+LF is needed for bios
print_crlf:
    pusha
    mov ah, 0x0e
    mov al, 0x0a
    int 0x10
    mov al, 0x0d
    int 0x10
    popa
    ret


; buffer for converting a word to hex string, to print it
print_hex_buffer: db '0x0000', 0

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
    add al, 0x41 - 0x3a  ; add extra value to go upto A-F range
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


; error messages
disk_error_msg: db 'Disk error', 0
mismatch_msg:   db 'Incorrect sectors read', 0

; function to load something from disk
; dl = drive number, dh = number of sectors, ES:BX points to target address
; see https://stanislavs.org/helppc/int_13-2.html
load_sectors:
    pusha
    ; also save dx to allow us to compare values read
    push dx

    mov ah, 0x02    ; int 0x13, function 0x02, read
    mov al, dh      ; number of sectors to read
    mov cl, 0x02    ; start from sector 2, as sector 1 already loaded by BIOS
    mov ch, 0x00    ; track/cylinder number
    ; dl already contains drive number (0=fdd0, 1=fdd1, 0x80=hdd0, 0x81=hdd1 etc)
    mov dh, 0x00    ; head number
    ; es:bx already points to target memory area
    int 0x13

    ; on error, carry is set
    jc load_sectors_disk_error
    pop dx
    cmp al, dh      ; see if we read all we wanted to
    jne load_sectors_mismatch

    popa
    ret

load_sectors_disk_error:
   mov bx, disk_error_msg
   call print_string
   call print_crlf
   ; error no will be in ah, dl arealdy contains the drive number
   mov dh, ah
   call print_hex
   ; freeze, not much to do
   jmp $

load_sectors_mismatch:
    mov bx, mismatch_msg
    call print_string
    call print_crlf
    ; freeze, not much to do
    jmp $



; Fill with 510 zeros minus the size of the previous code
times 510-($-$$) db 0

; Magic number for BIOS to understand we are a bootable sector
; must be the last two bytes of the sector
dw 0xaa55 
