; stage2_vbe.asm -- NASM, elf32, BITS 16
BITS 16

extern stage2_main

global _stage2_start
global vbe_set_mode_real
global vbe_get_mode_info_real
global bios_read_sectors_asm


; this label at address 0x800 / 2MB
_stage2_start:

    ; show we are running
    mov ah, 0x0E      ; teletype print
    mov al, 'S'
    int 0x10

    ; before calling C, setup some deeper stack
    ; we are at 0x800, or 2KB, and could go up to 31KB, where the first stage is.
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; now we can call the C code, still in real mode
    call stage2_main
    hlt
    jmp $



; ----------------------------------------------------
; vbe_set_mode_real
; Input: BX = mode
; Output: AL = 1 success, 0 fail
; ----------------------------------------------------
vbe_set_mode_real:
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail_set
    mov al, 1
    ret
.fail_set:
    xor al, al
    ret



; ----------------------------------------------------
; vbe_get_mode_info_real
; Input:
;   CX = mode
;   DX = linear address of 256-byte buffer (from C)
; Output:
;   AL = 1 success, 0 fail
; ----------------------------------------------------
vbe_get_mode_info_real:
    ; Convert linear address (DX) -> ES:DI
    mov ax, dx
    mov es, ax          ; segment = linear >> 4? We'll split correctly
    mov di, dx          ; offset = linear & 0xF ?

    ; Actually, split properly:
    mov bx, dx          ; save linear
    shr bx, 4           ; segment = linear >> 4
    and dx, 0x0F        ; offset = linear & 0xF
    mov es, bx
    mov di, dx

    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .fail_get
    mov al, 1
    ret
.fail_get:
    xor al, al
    ret



; ---------------------------------------------------------------------------
; uint8_t bios_read_sectors_asm(struct dap* packet)
;
; C passes: packet pointer in EDX   (SysV ABI, same as GCC -m16 backend)
; Returns: AL = 0 success, AL = 1 failure
; ---------------------------------------------------------------------------

bios_read_sectors_asm:
    ; Save registers we will use
    push ax
    push bx
    push cx
    push dx
    push si
    push ds

    ; EDX contains 32-bit pointer to DAP
    ; Convert it to segment:offset
    mov bx, dx          ; BX = lower 16 bits (offset)
    shr edx, 16
    mov ds, dx          ; DS = upper 16 bits (segment)
    mov si, bx          ; SI = offset

    ; BIOS extended read
    mov ah, 0x42        ; function: extended read
    mov dl, 0x80        ; boot disk = first HDD (use 0x00 for floppy)
    int 0x13
    jc .error           ; carry = error

    xor al, al          ; success: AL = 0
    jmp .done

.error:
    mov al, 1           ; failure: AL = 1

.done:
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax

    ret





; --------------------------------------------------------------------------------
; Moving to protected mode
; --------------------------------------------------------------------------------

[BITS 16]
global pm_entry

pm_entry:
    cli                 ; disable interrupts
    lgdt [gdt_descriptor] ; load GDT
    mov eax, cr0
    or eax, 1           ; set PE bit
    mov cr0, eax        ; enter protected mode
    jmp CODE_SEL:pm_entry_pm  ; far jump to flush prefetch

; ------------------------
[BITS 32]
pm_entry_pm:
    ; 1. Set up data segments
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 2. Set up stack
    mov ax, DATA_SEL
    mov ss, ax          ; load stack segment
    mov esp, 0x90000    ; stack top (match what kernel.ld has)

    ; jump to kernel entry point in EAX
    jmp eax

; ------------------------
[BITS 16]
align 8
gdt_start:
    ; null descriptor
    dd 0
    dd 0

    ; code segment descriptor
    dd 0x0000FFFF
    dd 0x00CF9A00

    ; data segment descriptor
    dd 0x0000FFFF
    dd 0x00CF9200
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; limit
    dd gdt_start                ; base

CODE_SEL equ 0x08
DATA_SEL equ 0x10

