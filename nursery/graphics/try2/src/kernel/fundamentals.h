#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

typedef signed char            s8;
typedef signed short int       s16;
typedef signed long int        s32;
typedef signed long long int   s64;
typedef unsigned char          u8;
typedef unsigned short int     u16;
typedef unsigned long int      u32;
typedef unsigned long long int u64;


#define min(a, b)                ((a)<(b) ? (a) : (b))
#define max(a, b)                ((a)>(b) ? (a) : (b))
#define clamp(value, lo, hi)     min(max((value), (lo)), (hi))
