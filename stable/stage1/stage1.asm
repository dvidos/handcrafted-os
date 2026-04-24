; 16 bit bootloader, loads stage2.bin

[BITS 16]
[ORG 0x7C00]

%ifndef STAGE2_LOAD_ADDRESS
    %error STAGE2_LOAD_ADDRESS not defined
%endif
%ifndef STAGE2_SECTORS
    %error STAGE2_SECTORS not defined
%endif


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
    mov bx, STAGE2_LOAD_ADDRESS
    mov dh, 0               ; head 0
    mov dl, 0x80            ; first hard disk
    mov ah, 0x02            ; BIOS read sectors
    mov al, STAGE2_SECTORS  ; number of sectors (each 512 bytes)
    mov ch, 0               ; cylinder 0
    mov cl, 2               ; sector 2 (sector numbers start at 1)
    int 0x13
    jc disk_error

    ; show we loaded
    mov ah, 0x0E      ; teletype print
    mov al, 'G'
    int 0x10

    ; jump to stage2 entry point
    jmp 0x0000:STAGE2_LOAD_ADDRESS

disk_error:
    ; show "Error"
    mov ah, 0x0E      ; teletype print
    mov al, 'E'
    int 0x10
    hlt
    jmp $

; Pad the boot code to exactly 446 bytes (offset 0x1BD)
; This leaves space for the 4 partition entries (64 bytes) and the boot signature (2 bytes).
times (0x1BE - ($ - $$)) db 0

; --- MBR Partition Table ---
; Partition table, each entry 16 bytes, at offsets 0x1BE, 0x1CE, 0x1DE, 0x1EE
partition1_data times 16 db 0
partition2_data times 16 db 0
partition3_data times 16 db 0
partition4_data times 16 db 0

; This must be 0xAA55 for the BIOS to recognize it as a bootable sector.
dw 0xAA55
