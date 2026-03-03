

/**
 * Central function to call int 80, observice our kernel's ABI
 * Essentially, the ABI says the following:
 * Regarding request:
 *   INT 0x80
 *   EAX will contain the sysno of the method to be called
 *   EBX will contain argument No 1
 *   ECX will contain argument No 2
 *   EDX will contain argument No 3
 *   ESI will contain argument No 4
 *   EDI will contain argument No 5
 * Regarding response:
 *   EAX will contain the response code. To be interpreted as a signed int of 32 bits
 */
int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5) {
    int return_value = 0;

    // maybe this helps: https://forum.osdev.org/viewtopic.php?f=1&t=9510&sid=b88edf5c775e3d8c05287a8c471c1986&start=15

    // the sections are: (assembly : output : input : clobbered registers)
    // the %n correspond to numbered variables, starting from the output section, zero based
    // the %%abc are names of registers
    // "g" lets compiler choose, "r" forces a register, "N" is literal value for inputs
    // "a"=EAX, "b"=EBX, "c"=ECX, "d"=EDX, "S"=ESI, "D"=EDI

    /*
        one can see what this block generates by running:
        "make && objdump -d bin/int80"

        55          push   %ebp
        89 e5       mov    %esp,%ebp
        57          push   %edi
        56          push   %esi
        53          push   %ebx
        83 ec 04    sub    $0x4,%esp          ; make room for local var "return_value"
        8b 45 08    mov    0x8(%ebp),%eax     ; ebp + 8 -> eax
        8b 5d 0c    mov    0xc(%ebp),%ebx     ; ebp + 12 -> ebc
        8b 4d 10    mov    0x10(%ebp),%ecx    ; ebp + 16 -> ecx
        8b 55 14    mov    0x14(%ebp),%edx    ; ebp + 20 -> edx
        8b 75 18    mov    0x18(%ebp),%esi    ; ebx + 24 -> esi
        8b 7d 1c    mov    0x1c(%ebp),%edi    ; ebx + 28 -> edi
        cd 80       int    $0x80
        89 45 f0    mov    %eax,-0x10(%ebp)   ; eax -> "return_value"
        8b 45 f0    mov    -0x10(%ebp),%eax   ; prepare to return the "return_value" from the function
        83 c4 04    add    $0x4,%esp          ; clean up local variable space
        5b          pop    %ebx
        5e          pop    %esi
        5f          pop    %edi
        5d          pop    %ebp
        c3          ret    
    */


    __asm__ __volatile__(
        "movl %1, %%eax \n\t"
        "movl %2, %%ebx \n\t"
        "movl %3, %%ecx \n\t"
        "movl %4, %%edx \n\t"
        "movl %5, %%esi \n\t"
        "movl %6, %%edi \n\t"
        "int $0x80 \n\t"
        "movl %%eax, %0 \n\t"
        : "=g" (return_value)
        : "g"(sysno), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "g"(arg5)
        : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
    );

    return return_value;
}

