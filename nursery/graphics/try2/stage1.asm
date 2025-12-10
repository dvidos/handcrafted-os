; 16 bit bootloader, loads stage2.bin

[BITS 16]
[ORG 0x7C00]

start:
    cli                     ; disable interrupts
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax              ; segment of stack
    mov sp, 0x8000          ; confines 1st stage in the 31KB-32KB area (0x7C00-0x8000), giving 512 bytes of usable stack

    ; show we are... "Loading"
    mov ah, 0x0E      ; teletype print
    mov al, 'L'
    int 0x10

    ; load stage2 at 0x800 (2 KB), about 16 sectors or 8 KB
    mov bx, 0x800
    mov dh, 0      ; head 0
    mov dl, 0x80   ; first hard disk
    mov ah, 0x02   ; BIOS read sectors
    mov al, 16     ; number of sectors (each 512 bytes)
    mov ch, 0      ; cylinder 0
    mov cl, 2      ; sector 2 (sector numbers start at 1)
    int 0x13
    jc disk_error

    ; show we loaded
    mov ah, 0x0E      ; teletype print
    mov al, 'G'
    int 0x10

    ; jump to stage2 entry point
    jmp 0x0000:0x0800

disk_error:
    ; show "Error"
    mov ah, 0x0E      ; teletype print
    mov al, 'E'
    int 0x10
    hlt
    jmp $

times 510-($-$$) db 0
dw 0xAA55
