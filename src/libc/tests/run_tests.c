#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*
    Turns out it is really difficult to test my libc the way I thought I could:
    I wanted to have a small C program that would compile against both the std libc,
    but also use pieces of my libc implementation.

    Well, my library is compiled for 32 bits (-m32), while my host system (including
    my libc) is compiled for 64 bits. 
    Therefore the linker cannot use both libraries for one executable.

    But, there is a package called gcc-multilib that one can install.

    Wait, we have the source code, why can't we compile it for host computer?
*/

#define assert(x)   if (!(x)) { exit(1); }

int main(int argc, char *argv[]) {
    char a[20];

    printf("Hardcoded-OS libc testing suite, running tests...\n");

    a[0] = '\0';
    assert(strlen(a) == 0);

    strcpy(a, "Hello world");
    assert(strlen(a) == 11);
    assert(strcmp(a, "Hello world") == 0);

    printf("Tada!\n");

    // return 1 in case of errors, causes make to exit(2)
    exit(0);
}

