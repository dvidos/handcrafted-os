[EXTERN interrupt_handler_c]


%macro ISR_ENTRY_WITHOUT_ERROR_CODE 1  ; define a macro, taking one parameter
  [GLOBAL isr%1]       ; %1 accesses the first parameter.
  isr%1:
    cli
    push dword  0     ; we push a zero, to make stack snapshot identical to when there is an error code
    push dword %1     ; we now push the isr number
    jmp isr_common_body
%endmacro

%macro ISR_ENTRY_WITH_ERROR_CODE  1
  [GLOBAL isr%1]
  isr%1:
    cli
    ; we don't push a zero, CPU already pushed the error code
    push dword %1    ; we now push the isr number
    jmp isr_common_body
%endmacro

%macro IRQ_ENTRY 1    ; define a macro, taking one parameter
  [GLOBAL irq%1]        ; %1 accesses the first parameter.
  irq%1:
    cli
    push dword  0     ; we push a zero, to make stack snapshot identical to when there is an error code
    push dword %1    ; we now push the isr number
    jmp isr_common_body
%endmacro


; for a list of specific codes, see https://wiki.osdev.org/Exceptions
ISR_ENTRY_WITHOUT_ERROR_CODE   0
ISR_ENTRY_WITHOUT_ERROR_CODE   1
ISR_ENTRY_WITHOUT_ERROR_CODE   2
ISR_ENTRY_WITHOUT_ERROR_CODE   3
ISR_ENTRY_WITHOUT_ERROR_CODE   4
ISR_ENTRY_WITHOUT_ERROR_CODE   5
ISR_ENTRY_WITHOUT_ERROR_CODE   6
ISR_ENTRY_WITHOUT_ERROR_CODE   7
ISR_ENTRY_WITH_ERROR_CODE      8
ISR_ENTRY_WITHOUT_ERROR_CODE   9
ISR_ENTRY_WITH_ERROR_CODE     10
ISR_ENTRY_WITH_ERROR_CODE     11
ISR_ENTRY_WITH_ERROR_CODE     12
ISR_ENTRY_WITH_ERROR_CODE     13
ISR_ENTRY_WITH_ERROR_CODE     14
ISR_ENTRY_WITHOUT_ERROR_CODE  15
ISR_ENTRY_WITHOUT_ERROR_CODE  16
ISR_ENTRY_WITHOUT_ERROR_CODE  17
ISR_ENTRY_WITHOUT_ERROR_CODE  18
ISR_ENTRY_WITHOUT_ERROR_CODE  19
ISR_ENTRY_WITHOUT_ERROR_CODE  20
ISR_ENTRY_WITHOUT_ERROR_CODE  21
ISR_ENTRY_WITHOUT_ERROR_CODE  22
ISR_ENTRY_WITHOUT_ERROR_CODE  23
ISR_ENTRY_WITHOUT_ERROR_CODE  24
ISR_ENTRY_WITHOUT_ERROR_CODE  25
ISR_ENTRY_WITHOUT_ERROR_CODE  26
ISR_ENTRY_WITHOUT_ERROR_CODE  27
ISR_ENTRY_WITHOUT_ERROR_CODE  28
ISR_ENTRY_WITHOUT_ERROR_CODE  29
ISR_ENTRY_WITHOUT_ERROR_CODE  30
ISR_ENTRY_WITHOUT_ERROR_CODE  31

; for a list of those, see https://en.wikipedia.org/wiki/Interrupt_request_%28PC_architecture%29
IRQ_ENTRY   32  ; system timer (cannot be changed)
IRQ_ENTRY   33  ; keyboard on PS/2 port (cannot be changed)
IRQ_ENTRY   34  ; 8259 interrupt controller; cascaded signals from IRQ_ENTRYs 8–15
IRQ_ENTRY   35  ; serial port controller for serial port 2 (shared with serial port 4, if present)
IRQ_ENTRY   36  ; serial port controller for serial port 1 (shared with serial port 3, if present)
IRQ_ENTRY   37  ; parallel port 3 or ISA sound card
IRQ_ENTRY   38  ; floppy disk controller
IRQ_ENTRY   39  ; parallel port 1 (shared with parallel port 2, if present)
IRQ_ENTRY   40  ; real-time clock (RTC)
IRQ_ENTRY   41  ; Advanced Configuration and Power Interface (ACPI) system control interrupt on Intel chipsets
IRQ_ENTRY   42  ; The interrupt is left for the use of peripherals (for example, SCSI or NIC)
IRQ_ENTRY   43  ; The interrupt is left for the use of peripherals (for example, SCSI or NIC)
IRQ_ENTRY   44  ; mouse on PS/2 port
IRQ_ENTRY   45  ; CPU co-processor or integrated floating point unit or inter-processor interrupt
IRQ_ENTRY   46  ; primary ATA channel (ATA interface usually serves hard disk drives and CD drives)
IRQ_ENTRY   47  ; secondary ATA channel
IRQ_ENTRY  128  ; i.e. 0x80, i.e. syscall


; This is the 2nd part of interrupt handling, starting with the macros
; It saves the processor state, sets up for kernel mode segments, 
; calls the C-level fault handler, and finally restores the stack frame.
; Caution, CPU state is stored as a C struct snapshot, keep them in sync
; see registers_t structure in C
isr_common_body:
  pusha          ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax
  push ds
  push es
  push fs
  push gs

  mov ax, 0x10  ; load the kernel data segment descriptor
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  push esp         ; pass a pointer to the stack
  call interrupt_handler_c
  add esp, 4       ; clean up passed arguments
   
  pop gs
  pop fs
  pop es
  pop ds
  popa                     ; Pops edi,esi,ebp...

  add esp, 8     ; cleans up the pushed error code and pushed ISR number (see macros)
  sti
  iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP


