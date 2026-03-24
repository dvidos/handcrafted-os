
extern tss_address
[global low_level_context_switch]
low_level_context_switch:
    ; TODO: this should be deprecated, once the other works.
    ; this method expects four arguments from C:
    ;   1. [esp+4]  a pointer to a location where to save the ESP of this process
    ;   2. [esp+8]  a pointer to a value to set ESP to, possibly returning to a different caller
    ;   3. [esp+12] a value to set CR3, if paging is desired
    ;   4. [esp+16] a value to set tss.esp0 to, for the CPU
    ; it also uses a global variable that points to the TSS instance

    mov eax, [esp+4]    ; old esp ptr
    mov ebx, [esp+8]    ; new esp ptr
    mov ecx, [esp+12]   ; page dir
    mov edx, [esp+16]   ; tss_esp0

    ; we do NOT modify ESP by pushing/popping as 
    ; we need to save/restore as best as possible
    
    mov [eax], esp              ; save old esp to outgoing process
    mov cr3, ecx                ; switch page directory to new process
    mov [tss_address+4], edx    ; set TSS
    mov esp, [ebx]              ; switch stack (esp based arguments are now gone)

    ret
