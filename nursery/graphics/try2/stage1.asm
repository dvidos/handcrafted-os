; 16 bit bootloader, loads stage2.bin

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; load stage2 at 0x1000
    mov bx, 0x1000
    mov dh, 0      ; sectors to read = 1 (simplified)
    mov dl, 0x80   ; first hard disk
    mov ah, 0x02   ; BIOS read sectors
    mov al, 1
    mov ch, 0
    mov cl, 2      ; LBA 1
    int 0x13

    ; jump to stage2
    jmp 0x1000

times 510-($-$$) db 0
dw 0xAA55
