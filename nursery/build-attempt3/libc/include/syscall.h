#ifndef _LIBC_SYSCALL_H
#define _LIBC_SYSCALL_H


#include "../../kernel/include/uapi/syscall.h"


int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5);





#endif
