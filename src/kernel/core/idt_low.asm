[GLOBAL load_idt_descriptor]    ; Allows the C code to call idt_flush().

load_idt_descriptor:
   mov eax, [esp+4]  ; Get the pointer to the IDT, passed as a parameter.
   lidt [eax]        ; Load the IDT pointer.
   ret

[EXTERN isr_handler]

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isr_common_stub:
   pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

   mov ax, ds               ; Lower 16-bits of eax = ds.
   push eax                 ; save the data segment descriptor

   mov ax, 0x10  ; load the kernel data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   ; as a debugging aid, try to print something at top of screen, then halt
   ; mov byte [gs:0xb8000], '['
   ; mov byte [gs:0xb8002], '!'
   ; mov byte [gs:0xb8004], ']'
   ;   hlt

   call isr_handler
   
   pop eax        ; reload the original data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa                     ; Pops edi,esi,ebp...
   add esp, 8     ; Cleans up the pushed error code and pushed ISR number
   sti
   iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP





%macro ISR_NOERRCODE 1  ; define a macro, taking one parameter
  [GLOBAL isr%1]        ; %1 accesses the first parameter.
  isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  [GLOBAL isr%1]
  isr%1:
    cli
    push byte %1
    jmp isr_common_stub
%endmacro

%macro IRQ 1  ; define a macro, taking one parameter
  [GLOBAL irq%1]        ; %1 accesses the first parameter.
  irq%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro


; for a list of specific codes, see https://wiki.osdev.org/Exceptions
ISR_NOERRCODE  0
ISR_NOERRCODE  1
ISR_NOERRCODE  2
ISR_NOERRCODE  3
ISR_NOERRCODE  4
ISR_NOERRCODE  5
ISR_NOERRCODE  6
ISR_NOERRCODE  7
ISR_ERRCODE    8
ISR_NOERRCODE  9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; for a list of those, see https://en.wikipedia.org/wiki/Interrupt_request_%28PC_architecture%29
IRQ 32
IRQ 33
IRQ 34
IRQ 35
IRQ 36
IRQ 37
IRQ 38
IRQ 39
IRQ 40
IRQ 41
IRQ 42
IRQ 43
IRQ 44
IRQ 45
IRQ 46
IRQ 47




[GLOBAL isr0x80]     ; allows setting up the IDT from C
[EXTERN isr_syscall] ; defined in C

; This is our syscall ISR handler. It saves the processor state, sets
; up for kernel mode segments, calls the C-level handler,
; and finally restores the stack frame.
isr0x80:
  cli

  ; in our libc, the syscall() method puts the arguments in:
  ; eax=sysno, ebx, ecx, edx, esi, edi = args 1-5
  ; pushing them so that our C isr handler can find them as arguments
  ; if you change this, change the arguments to the isr_syscall() C method accordingly
  push eax
  push ebx
  push ecx
  push edx
  push esi
  push edi

  ; then we'll use EDX to save and restore the segments
  ; and load the data segment descriptor of the kernel
  mov dx, ds    ; Lower 16-bits of edx = ds.
  push edx      ; save the data segment descriptor
  mov dx, 0x10  ; load the kernel data segment descriptor
  mov ds, dx
  mov es, dx
  mov fs, dx
  mov gs, dx

  ; as a debugging aid, try to print something at top of screen, then halt
  ; mov byte [gs:0xb8000], '['
  ; mov byte [gs:0xb8002], '!'
  ; mov byte [gs:0xb8004], ']'
  ; hlt

  call isr_syscall

  ; restore the original segment descriptors, without affecting eax (return value)
  pop edx
  mov ds, dx
  mov es, dx
  mov fs, dx
  mov gs, dx

  ; we have to clean up the values we pushed,
  ; but without affecting eax, which contains the return value
  ; we pushed 6 variabels of 4 bytes each, so add 24 to sp
  add esp, 24

  sti
  iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
