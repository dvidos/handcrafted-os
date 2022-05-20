
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
[global low_level_context_switch]
low_level_context_switch:
    ; this method expects two arguments:
    ;   1. a pointer to a location where to save the last ESP of this process
    ;   2. a pointer to a value to set ESP to, possibly returning to a different caller

    
    ; we want to avoid pushing EBP, because we think it ruins our switching
    ; when it is popped at the other side, especially on the first execution.
    ; so we try this:
    mov eax, [esp+4]  ; hopefully the first argument, since we pushed no EBP
    mov edx, [esp+8]  ; hopefully the second argument

    ; push ebp     ; [ebp+4] return address, +8 first arg, +12 second arg
    ; mov ebp, esp

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

    cmp eax, 0         ; don't save if a null pointer is given
    je no_old_esp
    mov [eax], esp     ; set the value to point to current SP
no_old_esp:

    cmp edx, 0
    je no_new_esp
    mov esp, [edx]     ; set the new stack pointer
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
    ; mov esp, ebp
    ; pop ebp

    ; we will now return to a differrent caller:
    ; whoever called the switching function when the first SP was saved.
    ret
