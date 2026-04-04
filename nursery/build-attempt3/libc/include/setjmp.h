#ifndef _SETJMP_H
#define _SETJMP_H

// Opaque type to hold the calling environment
typedef long jmp_buf[5]; // Size is platform/architecture dependent, 5 longs is a common placeholder

// --- Function Prototypes ---
// Saves the current environment (stack pointer, program counter, registers)
// into env and returns 0. If called via longjmp, it returns the value passed to longjmp.
int setjmp(jmp_buf env);

// Restores the environment saved by the most recent setjmp call.
// Execution resumes at the point of the setjmp call, which then returns val.
void longjmp(jmp_buf env, int val);

// A POSIX extension: sigsetjmp and siglongjmp. Not explicitly found in usage, but common.
// typedef long sigjmp_buf[...];
// int sigsetjmp(sigjmp_buf env, int savesigs);
// void siglongjmp(sigjmp_buf env, int val);

#endif // _SETJMP_H