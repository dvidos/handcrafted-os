#include <stdio.h>

// this one to create a data segment
int some_variable = 0;

int syscall(int arg1, int arg2) {
    (void)arg1;
    (void)arg2;
    int return_value = 0;
    __asm__ volatile("int $0x80" : "=a" (return_value));
    return return_value;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    syscall(12, 34);
    return 0;
}


