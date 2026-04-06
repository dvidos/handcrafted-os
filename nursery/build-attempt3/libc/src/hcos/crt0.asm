section .text
global _start
extern __libc_init

_start:
    ; [esp]    = return address
    ; [esp+4]  = argc
    ; [esp+8]  = argv
    ; [esp+12] = envp

    mov eax, esp        ; Keep a stable reference to the entry stack
    push dword [eax+12] ; Push envp
    push dword [eax+8]  ; Push argv
    push dword [eax+4]  ; Push argc
    
    call __libc_init    ; Now we have a proper C stack frame
                        ; __libc_init() will never return
    