

; verify if CPUID is available, see https://wiki.osdev.org/CPUID
; a non-zero returned value means it is supported
[GLOBAL get_cpuid_availability]
get_cpuid_availability:
    pushfd                               ;Save EFLAGS
    pushfd                               ;Store EFLAGS
    xor dword [esp],0x00200000           ;Invert the ID bit in stored EFLAGS
    popfd                                ;Load stored EFLAGS (with ID bit inverted)
    pushfd                               ;Store EFLAGS again (ID bit may or may not be inverted)
    pop eax                              ;eax = modified EFLAGS (ID bit may or may not be inverted)
    xor eax,[esp]                        ;eax = whichever bits were changed
    popfd                                ;Restore original EFLAGS
    and eax,0x00200000                   ;eax = zero if ID bit can't be changed, else non-zero
    ret


; given that CPUID capability exists, get whether cpu supports long mode
[GLOBAL get_long_mode_capability]
get_long_mode_capability:
    mov eax, 0x80000000         ; Set the A-register to 0x80000000.
    cpuid                       ; CPU identification.
    cmp eax, 0x80000001         ; Compare the A-register with 0x80000001.
    jb no_long_mode_capability  ; It is less, there is no long mode.
    mov eax, 1                  ; C gets return values from AL, AX, EAX, RAX
    ret
no_long_mode_capability:
    mov eax, 0
    ret
