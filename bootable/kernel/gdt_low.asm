
gdtr DW 0 ; For limit storage
     DD 0 ; For base storage

; method expecting a (void *) argument, then a size_t argument, called from C
; prepares the global descriptor table (GDT) pointer
[GLOBAL load_gdt_entries]
load_gdt_entries:
   MOV   EAX, [esp + 4]
   MOV   [gdtr + 2], EAX
   MOV   AX, [ESP + 8]
   MOV   [gdtr], AX
   LGDT  [gdtr]
   RET

; these two methods have knowledge from inside the kernel.
; they know that gdtr+8 is our code segment and gdtr+16 is our data segment
reload_remaining_gdt_segments:
   ; Reload data segment registers:
   MOV   AX, 0x10 ; 0x10 points at the new data selector
   MOV   DS, AX
   MOV   ES, AX
   MOV   FS, AX
   MOV   GS, AX
   MOV   SS, AX
   RET

[GLOBAL reload_gdt_segments]
reload_gdt_segments:
   ; Reload CS register containing code selector
   ; We have to do a far jump, in order for CS to be populated.
   JMP   0x08:reload_remaining_gdt_segments ; 0x08 points at the new code selector
