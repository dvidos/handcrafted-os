#ifndef _SYSCALL_H
#define _SYSCALL_H


#ifdef __is_libc
    int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5);
#endif


// number of syscall method -- to be shared with kernel.
#define SYS_PUTS     1
#define SYS_PUTCHAR  2








#endif

