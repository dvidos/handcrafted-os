#pragma once
#ifdef __KERNEL__

    typedef unsigned char uint8_t;
    typedef unsigned short int uint16_t;
    typedef unsigned long int uint32_t;
    typedef unsigned long long int uint64_t;

    typedef signed char int8_t;
    typedef signed short int int16_t;
    typedef signed long int int32_t;
    typedef signed long long int int64_t;

#else 
    #include <stdint.h>

#endif
