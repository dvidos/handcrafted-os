[extern interrupt_handler_c]


%macro ISR_ENTRY_WITHOUT_ERROR_CODE 1  ; define a macro, taking one parameter
  [global isr%1]       ; %1 accesses the first parameter.
  isr%1:
    cli
    push dword  0     ; we push a zero, to make stack snapshot identical to when there is an error code
    push dword %1     ; we now push the isr number
    jmp isr_common_body
%endmacro

%macro ISR_ENTRY_WITH_ERROR_CODE  1
  [global isr%1]
  isr%1:
    cli
    ; we don't push a zero, CPU already pushed the error code
    push dword %1    ; we now push the isr number
    jmp isr_common_body
%endmacro

%macro IRQ_ENTRY 1    ; define a macro, taking one parameter
  [global irq%1]        ; %1 accesses the first parameter.
  irq%1:
    cli
    push dword  0     ; we push a zero, to make stack snapshot identical to when there is an error code
    push dword %1    ; we now push the isr number
    jmp isr_common_body
%endmacro


%macro PROCESS_SWITCHER_STUB 0   ; a macro for switching tasks, based on global variables
    [extern proc_switch_needed]
    [extern proc_switch_old_esp_ptr]
    [extern proc_switch_new_cr3]
    [extern proc_switch_new_tss_esp0]
    [extern proc_switch_new_esp]
    [extern proc_switch_tss_address]

    ; based on five global variables:
    ; - proc_switch_needed        int         whether we need to switch or not.
    ; - proc_switch_old_esp_ptr   uint32_t    address where to save the current ESP
    ; - proc_switch_new_esp       uint32_t    value of the new ESP to set
    ; - proc_switch_new_cr3       uint32_t    value of the new page directory to set
    ; - proc_switch_new_tss_esp0  uint32_t    value of the new tss.esp0 to set

    ; Check if a process switch is requested and perform it
    cmp byte [proc_switch_needed], 0
    je proc_switch_not_needed

    ; Save current ESP to the pointer
    mov eax, [proc_switch_old_esp_ptr]    ; eax = pointer to old ESP
    mov [eax], esp                        ; save current stack pointer

    ; Set new TSS.esp0
    mov edx, [proc_switch_tss_address]    ; edx = 0x2C4B0
    mov eax, [proc_switch_new_tss_esp0]   ; eax = 0x1954BD
    mov [edx + 4], eax                    ; Write 0x1954BD to 0x2C4B4

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ; [extern debug_one_dword]
    ; push dword [0x2c4b4]
    ; call debug_one_dword
    ; add esp,4
    ; push dword [proc_switch_new_tss_esp0]
    ; call debug_one_dword
    ; add esp,4
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ; Switch to new CR3 (page directory)
    mov eax, [proc_switch_new_cr3]
    mov cr3, eax

    ; Switch to new ESP
    mov esp, [proc_switch_new_esp]

    ; Clear the flag
    mov byte [proc_switch_needed], 0

proc_switch_not_needed:
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



[global isr_body_exit_point]
; This is the 2nd part of interrupt handling, starting with the macros
; It saves the processor state, sets up for kernel mode segments, 
; calls the C-level fault handler, and finally restores the stack frame.
; Caution, CPU state is stored as a C struct snapshot, keep them in sync
; see trap_frame_t structure in C
isr_common_body:
  pushad         ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax
  push ds
  push es
  push fs
  push gs

  mov ax, 0x10  ; load the kernel data segment descriptor
  mov ds, ax    ; code segment was already changed by cpu and int
  mov es, ax
  mov fs, ax
  mov gs, ax

  push esp         ; pass a pointer to the stack (to be used as a trapframe)
  call interrupt_handler_c
  add esp, 4       ; clean up passed arguments

isr_body_exit_point:  ; new processes "return" here via the 'minimal_returning_function'
  pop gs
  pop fs
  pop es
  pop ds
  popad          ; Pops edi,esi,ebp,esp,ebx,edx,ecx,eax

  add esp, 8     ; cleans up the pushed error code and pushed ISR number (see macros)
  sti
  iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP



; used in stack frames for starting new processes, by pushing the 'isr_body_exit_point' address to return to
[global minimal_returning_function]
minimal_returning_function:
  ret  ; will just to whatever return address is at top of stack, same segment / ring.



; extern void switch_inside_c_function(uint32_t *old_esp_ptr, uint32_t new_esp, uint32_t new_cr3, uint32_t new_esp0);
[global switch_inside_c_function]
[extern tss_address]
switch_inside_c_function:
  ; the 'call' instruction already pushed EIP here, push the rest of the c_frame_t
  push ebp
  push ebx
  push esi
  push edi
  ; already 5 values pushed, 4 bytes each = 20 bytes to arguments pushed

  ; save current ESP to old_process->saved_esp
  mov eax, [esp + 20]     ; First arg: old_esp_ptr
  mov [eax], esp

  ; switch page directory
  mov eax, [esp + 28]     ; Third arg: new_cr3
  mov cr3, eax

  ; assume esp0 at 4 bytes inside the tss structure
  mov eax, [esp + 32]     ; Fourth arg: new_esp0
  mov edx, [tss_address]
  mov [edx + 4], eax

  ; switch to the new stack
  mov esp, [esp + 24]     ; Second arg: new_esp

  ; restore the new process' c_frame_t
  pop edi
  pop esi
  pop ebx
  pop ebp

  ; 6. Resumes the EIP (either into C code or minimal_returning_function)
  ret   ; pops the return address, jumps there
