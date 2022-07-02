#include <stdio.h>
#include <stdint.h>

// this one to create a .bss (zero initialized) segment
int some_variable = 0;

// this one to create a .data (non-zero initialized) segment
int some_other_variable = 126;


// Central function to call int 80, observice our kernel's ABI
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

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int a = 0;
    a = syscall(1, 0x22222222, 0x33333333, 0x44444444, 0x55555555, 0x66666666);
    a = syscall(a, 0x77777777, 0x88888888, 0x99999999, 0xaaaaaaaa, 0xbbbbbbbb);
    a = syscall(a, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd, 0xeeeeeeee, 0xffffffff);

    return a;
}


