
[GLOBAL load_gdt_descriptor]
load_gdt_descriptor:
    mov eax, [esp+4]  ; Get the pointer to the GDT, passed as a parameter.
    lgdt [eax]        ; Load the GDT pointer.
    ; Reload CS register containing code selector
    ; We have to do a far jump, in order for CS to be populated.
    JMP   0x08:continue_reloading_gdt_segments ; 0x08 points at the new code selector
continue_reloading_gdt_segments:
    ; Reload data segment registers:
    MOV   AX, 0x10 ; 0x10 points at the new data selector
    MOV   DS, AX
    MOV   ES, AX
    MOV   FS, AX
    MOV   GS, AX
    MOV   SS, AX
    RET
