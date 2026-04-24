#ifndef _STDINT_H
#define _STDINT_H

#include <sys/types.h> // For uint8_t, int8_t, etc.

// --- Fixed-width integer types (from sys/types.h) ---
// typedef unsigned char   uint8_t;
// typedef signed char     int8_t;
// typedef unsigned short  uint16_t;
// typedef signed short    int16_t;
// typedef unsigned int    uint32_t;
// typedef signed int      int32_t;
// typedef unsigned long   uint64_t;
// typedef signed long     int64_t;
// typedef unsigned long   uintptr_t;
// typedef signed long     intptr_t;

// --- Minimum and Maximum values for fixed-width integer types ---

// 8-bit signed
#define INT8_MIN   (-128)
#define INT8_MAX   (127)
#define UINT8_MAX  (255U)

// 16-bit signed
#define INT16_MIN  (-32768)
#define INT16_MAX  (32767)
#define UINT16_MAX (65535U)

// 32-bit signed
#define INT32_MIN  (-2147483647L - 1)
#define INT32_MAX  (2147483647L)
#define UINT32_MAX (4294967295UL)

// 64-bit signed (assuming long is 64-bit)
#define INT64_MIN  (-9223372036854775807LL - 1)
#define INT64_MAX  (9223372036854775807LL)
#define UINT64_MAX (18446744073709551615ULL)

// --- Limits of integer types capable of holding object pointers ---
// These are platform-dependent; assuming same as signed/unsigned long
#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX


// This is a 32 bit system
#define SIZE_MAX	(4294967295U)



#endif // _STDINT_H