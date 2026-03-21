[GLOBAL load_idt_descriptor]    ; Allows the C code to call idt_flush().

load_idt_descriptor:
   mov eax, [esp+4]  ; Get the pointer to the IDT, passed as a parameter.
   lidt [eax]        ; Load the IDT pointer.
   ret

