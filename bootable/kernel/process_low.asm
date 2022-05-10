
; somehow this method will allow context switching
; it takes two pointers to two structures:
; the old context and the new context.
; it saves the context it was called from into the old context
; it restores the new context onto the stack (including SS)
; therefore, return will look like far jumping to the new context.
; our call to this function will look like blocked, 
; until someone puts our old context as their new context
;
; vx6 maintains a context per process and a context for the scheduler,
; ping-ponging between them as needed
;
; for example, in an interrupt, we may yield() to schedule another process
; yield() calls schedule(), which calls switch_context(), to allow it to 
; save the context of the "yield" caller, and "return" to the scheduler's context
; inversely, the scheduler will "switch_context" between itself and the process it chose
; to execute.
; 
; essentially, from the perspective of a process, calling "switch_context"
; (through any syscall method) blocks until the result is returned.
; from the perspective of a scheduler, it calls "switch_context" to a process
; and it blocks until the process gives up control or is sleeping.
;
; see https://github.com/mit-pdos/xv6-public/blob/master/swtch.S as well
;

; another idea is: save the current SP in memory, 
; then reload a new SS:ESP pointer to a new stack block. 
; The caller's EIP is stored in the original stack (it remains somewhere)
; and the new EIP is already pushed in the new stack 
; and will be popped off when the function returns.

; essentially, I think that what we do is the following:
; push things in the stack, return a pointer to that stack 
; that snapshot in time, to be later resumed.
; then load a new value onto SS:ESP, a pointer 
; that was previously loaded.

; keep in mind that when we dump stack, we should dump 
; in a *decreasing* order of bytes, therefore deper nested things 
; are towards the bottom
; otherwise it's too confusing, pushed values seem out of order
; e.g. pushing 0x11223344 seems like "4433 2211"
; 
; when calling a method, C pushes the arguments from right to left.
; e.g. if we have func(0xAA, 0xBB, 0xCC) 
; then 0xCC is pushed first, 0xBB second, 0xAA is pushed last.
; since stack grows downward, 0xAA is at lower address than 0xCC
;
; after the arguments, the return address is pushed (4 bytes in 32 bit systems)
; notice that the return address is larger than the function address,
; since we have all the code that led to the calling of the sub-function
;
; (inside the function)
; it seems the ESP is pushed next. maybe for the "push bp, mov bp, sp"
; this means that EBP will be pointing to the same spot for this method,
; and is used to refer to arguments and local variables, e.g. "mov ax, [bp+4]"
; offsets are negative for local variables, positive for arguments
; then the local variables are allocated (pushed) next
; 
; interrupts use IRET instead of RET, that pops another 4 bytes off the stack,
; as the EFLAGS were pushed when calling the interrupt handler
;
; nice explanation of stack and stack frames with EBP: https://wiki.osdev.org/Stack




; it boils down to this:
;   1. push a bunch of things in the stack,
;   2. save current SP to a location pointed by an argument of the C kernel,
;   3. give SP the value that C kernel passed to us,
;   4. pop the bunch of things in the opposite order.
[global simpler_context_switch]
simpler_context_switch:
    ; this method expects two arguments:
    ;   1. a pointer to a location where to save the last ESP of this process
    ;   2. a pointer to a value to set ESP to, possibly returning to a different caller

    push ebp     ; [ebp+4] return address, +8 first arg, +12 second arg
    mov ebp, esp

    ; save values to the stack, to preserve for next time
    pushfd    ; pushes 32bits flags
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    ; if we push something else here, we need to update the revelant C structure

    ; having pushed EAX we can use it for work.
    mov eax, [ebp+8]   ; EAX now contains the address of location to save ESP to
    cmp eax, 0         ; don't save if a null pointer is given
    je no_old_esp
    mov [eax], esp     ; set the value to point to current SP
no_old_esp:

    ; now we can load the other ESP
    mov eax, [ebp+12]  ; eax now contains the pointer to the value to set ESP to
    cmp eax, 0
    je no_new_esp
    mov esp, [eax]     ; set the new stack pointer
no_new_esp:

    ; restore whatever we had saved before
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    popfd
    
    ; clean up stack frame
    mov esp, ebp
    pop ebp

    ; we will now return to a differrent caller:
    ; whoever called the switching function when the first SP was saved.
    ret





; this method evolved from testing things out
; by returning EAX, I managed to inderstand how to reference the arguments
[global grab_some_registers]
grab_some_registers:
    push ebp
    mov ebp, esp

    ; these lines have been verified.
    ;                         ; [ebp+4] is the return address
    ; mov eax, [ebp+8]        ; [ebp+8] is the address of first argument!!
    ; mov eax, [ebp+12]       ; [ebp+12] is the address of next argument!!
    ; mov dword [eax], 44h    ; eax points to the first dword in the structure
    ; mov dword [eax+4], 55h  ; eax+4 points to the second dword in the structure etc
    ;                         ; [ebp-offset] negative offsets are local variables
    
    push eax                ; store this, we'll need a work register
    mov eax, [ebp+8]         ; store EAX in structure base address
    mov [eax+4], ebx
    mov [eax+8], ecx
    mov [eax+12], edx
    mov [eax+16], esp
    mov [eax+20], ebp        ; bp is already globbed though
    mov [eax+24], esi
    mov [eax+28], edi

    pop eax
    pop ebp
    mov esp, ebp       ; this cleans up the SP, no matter how many things we.ve push
    ret
