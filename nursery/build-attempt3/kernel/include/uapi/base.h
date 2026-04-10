#pragma once
// minimal definitions shared between kernel, libc, tools etc.


#ifdef __HOST_SYSTEM__
    // these definitions below for compiling on the host system
    // for example the sfs_tool cli tool
    #include <stdint.h>
    #include <stdbool.h>
    #include <stddef.h>
#else

    // these definitions below for kernel, libc, user apps etc

    typedef unsigned char uint8_t;
    typedef unsigned short int uint16_t;
    typedef unsigned long int uint32_t;
    typedef unsigned long long int uint64_t;

    typedef signed char int8_t;
    typedef signed short int int16_t;
    typedef signed long int int32_t;
    typedef signed long long int int64_t;

#endif

// in both cases, these must be specific sizes
_Static_assert(sizeof(uint8_t)  == 1, "uint8_t is expected to have size of 1 byte");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t is expected to have a size of 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t is expected to have a size of 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t is expected to have a size of 8 bytes");
