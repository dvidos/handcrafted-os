#pragma once

// minimal definitions shared between kernel, libc, tools etc.
#ifdef __KERNEL__

    // these definitions below for compiling as a kernel
    typedef unsigned char uint8_t;
    typedef unsigned short int uint16_t;
    typedef unsigned long int uint32_t;
    typedef unsigned long long int uint64_t;

    typedef signed char int8_t;
    typedef signed short int int16_t;
    typedef signed long int int32_t;
    typedef signed long long int int64_t;

#else

    // these definitions below for compiling on a host arch
    #include <stdint.h>
    #include <stdbool.h>
    #include <stddef.h>

#endif

// in any case, these must be fixed size
_Static_assert(sizeof(uint8_t)  == 1, "uint8_t is expected to have size of 1 byte");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t is expected to have a size of 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t is expected to have a size of 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t is expected to have a size of 8 bytes");
