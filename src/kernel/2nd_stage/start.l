     1                                  ; assembly file used to enable jumping to specific address for the second stage boot loader
     2                                  ; purpose is to save the boot drive number, setup stack, then jump to the C main() function
     3                                  [bits 16]
     4                                  
     5                                  
     6                                  [EXTERN main]           ; this is declared in the C code
     7                                  
     8                                  
     9                                  ; [global _start]   
    10                                  ; _start:
    11                                      ; just to make sure. 
    12 00000000 FA                          cli
    13                                      
    14                                      ; stack top will be 0x0FFF0, 16 bytes below the address of the 2nd stage loader (0x10000)
    15                                      ; SS will be 0x00, SS will be 0xFFF0
    16 00000001 B80000                      mov ax, 0x0
    17 00000004 8ED0                        mov ss, ax
    18 00000006 B8F0FF                      mov ax, 0xFFF0
    19 00000009 89C4                        mov sp, ax
    20                                  
    21                                      ; now we can jump to our C version of boot loader
    22 0000000B B80010                      mov ax, 0x1000     ; Code loaded at 64K
    23 0000000E 8EC8                        mov cs, ax
    24                                  
    25 00000010 E8(0000)                    call main
    26 00000013 EBFE                        jmp $                   ; freeze, main() is not supposed to return...
    27                                  
    28                                  
