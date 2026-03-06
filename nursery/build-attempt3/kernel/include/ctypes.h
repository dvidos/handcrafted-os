#pragma once


#ifdef __KERNEL__  // if compilation is for kernel

    #include "uapi/base.h"
    
    #define INT8_MIN    -128
    #define INT8_MAX    127
    #define INT16_MIN   -32768	
    #define INT16_MAX   32767
    #define INT32_MIN   -2147483648
    #define INT32_MAX   2147483647
    #define INT64_MIN   -9223372036854775808	
    #define INT64_MAX   9223372036854775807	

    #define UINT8_MAX   255U
    #define UINT16_MAX  65535U
    #define UINT32_MAX  4294967295LU
    #define UINT64_MAX  18446744073709551615LLU

    #define bool     _Bool
    #define true	((_Bool)1)
    #define false	((_Bool)0)

    #define NULL    (void *)0

    // semantic types (short=16 bits, long=32 bits)
    typedef unsigned long  int size_t;      // unsigned size of objects in memory, 32 bit
    typedef          long  int ssize_t;     // signed size, for I/O return errors as negative values
    typedef          long  int off_t;       // signed offset
    typedef unsigned short int mode_t;      // file mode (dir/file), permissions etc
    typedef unsigned long  int uid_t;       // user id
    typedef unsigned long  int gid_t;       // group id
    typedef          long  int pid_t;       // prod id. Signed allows for special in wait() etc
    typedef          long  int tid_t;       // thread id. (tid=pid for main thread)


    // find member offset. GCC has "__builtin_offsetof(type, member)", others do not.
    #define offsetof(type, member)     ((size_t)((char *)&(((type *)0)->member) - (char *)0))

    // get pointer to the parent structure that contains the pointed member, for mixins
    #define container_of(member_ptr, type, member)    (type *)((char *)member_ptr - offsetof(type, member))

#else  // if compilation is for hosted apps
    #include <stdbool.h>
    #include <stdint.h>
    #include <stddef.h>
    #include <stdio.h>
    #include <sys/types.h>
#endif



typedef unsigned long  int phys_addr_t; // physical address. 32 bits -> 4 GB
typedef unsigned long  int virt_addr_t; // virtual address. 32 bits -> 4 GB


