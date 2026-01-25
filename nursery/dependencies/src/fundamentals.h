#pragma once

// #define HOSTED // for now
#define STANDALONE // for now

// -----------------------------------------------------

// these should be passed from makefile (-DSTANDALONE)
#if !defined(STANDALONE) && !defined(HOSTED)
    #error At least one of STANDALONE or HOSTED must be defined
#endif
#if defined(STANDALONE) && defined(HOSTED)
    #error Only one of STANDALONE or HOSTED must be defined
#endif

// these should come from -m32 or -m64 compiler flag
#if __SIZEOF_POINTER__ == 8
    #define ARCH_64
#elif __SIZEOF_POINTER__ == 4
    #define ARCH_32
#else
    #error Unsupported pointer size
#endif

// -----------------------------------------------------


#ifdef HOSTED
    #include <stddef.h>
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdarg.h>

    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    // offsetof defined in stddef.h
    #define container_of(ptr, type, member)   ((type *)((char *)(ptr) - offsetof(type, member)))

#endif
#ifdef STANDALONE
    #include <stdarg.h>

    #if defined(ARCH_32)
        // ints will be 32 bits in -m32
        typedef unsigned int size_t;
        typedef signed   int ssize_t;
        typedef unsigned int uintptr_t;
        typedef unsigned int paddr_t;
        typedef unsigned int vaddr_t;
    #elif defined(ARCH_64)
        // longs will be 64 bits in -m64
        typedef unsigned long int size_t;
        typedef signed   long int ssize_t;
        typedef unsigned long int uintptr_t;
        typedef unsigned long int paddr_t;
        typedef unsigned long int vaddr_t;
    #endif

    #define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
    #define container_of(ptr, type, member)   ((type *)((char *)(ptr) - offsetof(type, member)))

    #define NULL ((void *)0)

    #define true    1
    #define false   0
    typedef _Bool bool;  // _Bool should be supported by compiler

    typedef unsigned char u8;
    typedef unsigned short int u16;
    typedef unsigned long int u32;
    typedef unsigned long long int u64;

#endif
