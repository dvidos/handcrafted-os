#ifndef _LIBC_INTERNAL_H
#define _LIBC_INTERNAL_H

#include "../include/sys/stat.h"
#include "../include/sys/time.h"
#include "../include/sys/types.h"
#include "../include/sys/wait.h"

#include "../include/assert.h"
#include "../include/ctype.h"
#include "../include/dirent.h"
#include "../include/errno.h"
#include "../include/fcntl.h"
#include "../include/float.h"
#include "../include/inttypes.h"
#include "../include/limits.h"
#include "../include/math.h"
#include "../include/setjmp.h"
#include "../include/signal.h"
#include "../include/stdarg.h"
#include "../include/stdbool.h"
#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/time.h"
#include "../include/unistd.h"
#include "../include/utime.h"


// syscall numbers
#include "../../kernel/include/uapi/syscall.h"
int syscall(int sysno, int arg1, int arg2, int arg3, int arg4, int arg5);











#endif // _LIBC_INTERNAL_H