; stage2_vbe.asm -- NASM, elf32, BITS 16
BITS 16

extern stage2_main
extern boot_info

global _stage2_start
global vbe_set_mode_real
global vbe_get_mode_info_real
global bios_read_sectors_asm

%ifndef STAGE2_STACK_TOP
    %error STAGE2_STACK_TOP not defined
%endif
%ifndef KERNEL_STACK_TOP
    %error KERNEL_STACK_TOP not defined
%endif
%ifndef KERNEL_LOAD_ADDRESS
    %error KERNEL_LOAD_ADDRESS not defined
%endif



; this label at address 0x800 / 2KB
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
    mov sp, STAGE2_STACK_TOP

    ; now we can call the C code, still in real mode
    call stage2_main
    hlt
    jmp $


global asm_return_bp2_arg   ; seems [BP+2]  lo word of return address
global asm_return_bp4_arg   ; seems [BP+4]  hi word of return address
global asm_return_bp6_arg   ; seems [BP+6]  lo word of 1st arg (leftmost)
global asm_return_bp8_arg   ; seems [BP+8]  hi word of 1st arg (leftmost)
global asm_return_bp10_arg  ; seems [BP+10] lo word of 2nd arg
global asm_return_bp12_arg  ; seems [BP+12] hi word of 2 arg

asm_return_bp2_arg:
    push bp
    mov bp, sp
    mov ax, [bp+2]
    mov sp,bp
    pop bp
    ret
asm_return_bp4_arg:
    push bp
    mov bp, sp
    mov ax, [bp+4]
    mov sp,bp
    pop bp
    ret
asm_return_bp6_arg:
    push bp
    mov bp, sp
    mov ax, [bp+6]
    mov sp,bp
    pop bp
    ret
asm_return_bp8_arg:
    push bp
    mov bp, sp
    mov ax, [bp+8]
    mov sp,bp
    pop bp
    ret
asm_return_bp10_arg:
    push bp
    mov bp, sp
    mov ax, [bp+10]
    mov sp,bp
    pop bp
    ret
asm_return_bp12_arg:
    push bp
    mov bp, sp
    mov ax, [bp+12]
    mov sp,bp
    pop bp
    ret
    


; ----------------------------------------------------
; vbe_get_ctrl_info_real
; Input: 4 bytes linear address of 512-byte buffer, pushed on stack
; Output: AL = 1 success, 0 failure
; Clobbers: BX, CX, DI, ES
; ----------------------------------------------------
global vbe_get_ctrl_info_real
vbe_get_ctrl_info_real:
    
    ; --- standard function prologue ---
    push bp           ; save old BP, establish stack frame. [BP] is old BP, [BP+2/+4] is return addr, ...
    mov bp, sp        ; [BP+6/+8] first arg low/high word, [BP+10/+12] second arg low/high word
    sub sp, 0         ; allocate n local bytes. [BP-2], [BP-4] etc
    ; --- save registers to destroy ---
    push bx
    push es
    push di
                      
    ; load arg into ES:DI (hand coded dv)
    mov dx, [bp+8]      ; load high word value...
    mov es, dx          ; ...onto es
    mov dx, [bp+6]      ; load low word value...
    mov di, dx          ; ...onto di

    ; VBE BIOS function 0x4F00
    mov ax, 0x4F00
    int 0x10

    ; set AL = 1 on success, 0 on failure
    cmp ax, 0x004F
    sete al

    ; restore registers (inverse)
    pop di
    pop es
    pop bx
    ; --- standard function epilogue ---
    mov sp,bp         ; deallocate local stack space
    pop bp            ; restore old BP
    ret               ; return to caller



global vbe_get_mode_info_real
vbe_get_mode_info_real:
    ; DX = linear buffer address
    ; CX = mode

    ; Convert linear address DX -> ES:DI
    mov bx, dx
    shr bx, 4
    and dx, 0x0F
    mov es, bx
    mov di, dx

    ; Call VBE BIOS function 0x4F01
    mov ax, 0x4F01
    int 0x10

    cmp ax, 0x004F
    sete al      ; AL = 1 on success, 0 otherwise
    ret

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








; ---------------------------------------------------------------------------
; uint8_t bios_read_sectors_asm(struct dap* packet)
;
; C passes: packet pointer in EDX   (SysV ABI, same as GCC -m16 backend)
; Returns: AL = 1 success, AL = 0 failure
; ---------------------------------------------------------------------------

bios_read_sectors_asm:
    ; Save registers we will use
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
    mov ax, 1           ; success: AL = 1
    jmp .done
.error:
    mov ax, 0           ; failure: AL = 0
.done:
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    ret





; --------------------------------------------------------------------------------
; Moving to protected mode
; --------------------------------------------------------------------------------

[BITS 16]
global enter_protected_mode

enter_protected_mode:
    cli                 ; disable interrupts
    lgdt [gdt_descriptor] ; load GDT
    mov eax, cr0
    or eax, 1           ; set PE bit
    mov cr0, eax        ; enter protected mode
    jmp CODE_SEL:enter_protected_mode_32bits  ; far jump to flush prefetch

; ------------------------
[BITS 32]
enter_protected_mode_32bits:
    ; 1. Set up data segments
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 2. Set up stack
    mov ax, DATA_SEL
    mov ss, ax          ; load stack segment
    mov esp, KERNEL_STACK_TOP   ; stack top (match what kernel.ld has)
    
    ; pass in first argument in kernel_main()
    mov eax, boot_info
    push eax

    ; jump to kernel entry point in EAX
    mov eax, KERNEL_LOAD_ADDRESS
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
