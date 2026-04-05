#include <setjmp.h>
#include <stddef.h> // For size_t, NULL
#include <stdbool.h> // For bool

int setjmp(jmp_buf env) {
    return 0;
}

void longjmp(jmp_buf env, int val) {
}

