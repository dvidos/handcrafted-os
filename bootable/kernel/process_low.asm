
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
[global switch_context_low_level]
switch_context_low_level:
    ; we expect two pointers to be passed, if they are passed in the stack,
    ; I think [ESP+4] should be the first pointer, [ESP+8] the second.
    ; [ESP] (without modifiers) should point to the return address, which is pushed last
    ;
    ; another way I saw it (in x86_64 arch) was one arg pointed by ESI, the other by EDI
    ; https://github.com/tiqwab/xv6-x86_64/blob/5a5ec6c3f0d44a14674248ba1cdf201f3da93ee1/kern/swtch.S
    ; 
    ; to walk the structure,
    ; one trick I saw is that people make ESP point to the C struct,
    ; then push or pop the registers one by one, therefore ESP acts 
    ; as the incremented pointer into the structure items.

    ; an idea to experiment: 1:save register to stack, 
    ; 2:have register point to the struct, 3:save all state to struct
    ; 4:retrieve register to save in struct
    push ebp
    mov ebp, esp

    mov eax, [ebp+8]  ; [ebp+8] is the address of prev!!!!!
    mov eax, [ebp+12] ; [ebp+12] is the address of next!!!!!
    
    mov eax, [ebp+8]        ; load the prev structure first
    mov dword [eax], 22h    ; eax points to the first dword in the structure
    mov dword [eax+4], 33h  ; eax+4 points to the second dword in the structure

    mov eax, [ebp+12]       ; load the next structure first
    mov dword [eax], 44h    ; eax points to the first dword in the structure
    mov dword [eax+4], 55h  ; eax+4 points to the second dword in the structure

    
    ; mov eax, [ebp+4]   ; eax now points to prev
    ; mov dword [eax], 55h
    ; mov dword [eax+4], 66h

    ; mov eax, [ebp+8]   ; eax not points to next
    ; mov dword [eax], 77h
    ; mov dword [eax+4], 88h

    
    pop ebp
    ret


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



