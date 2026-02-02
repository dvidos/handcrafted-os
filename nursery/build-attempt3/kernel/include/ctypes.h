#ifndef _CTYPES_H
#define _CTYPES_H


// essentially, get rid of stdint and stdbool.
// become super independent
// specify

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned long int uint32_t;
typedef unsigned long long int uint64_t;

typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed long int int32_t;
typedef signed long long int int64_t;

typedef unsigned char uchar;
typedef unsigned int uint;

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;

typedef signed char s8;
typedef signed short int s16;
typedef signed long int s32;
typedef signed long long int s64;

#define INT8_MIN    -128
#define INT8_MAX    127
#define INT16_MIN   -32768	
#define INT16_MAX   32767
#define INT32_MIN   -2147483648
#define INT32_MAX   2147483647
#define INT64_MIN   -9223372036854775808	
#define INT64_MAX   9223372036854775807	

#define UINT8_MAX   255
#define UINT16_MAX  65535
#define UINT32_MAX  4294967295
#define UINT64_MAX  18446744073709551615


#define bool     _Bool
#define true	((_Bool)1)
#define false	((_Bool)0)

#define NULL    ((void *)0)

// semantic types (short=16 bits, long=32 bits)
typedef unsigned long  int size_t;      // unsigned size of objects in memory, 32 bit
typedef          long  int ssize_t;     // signed size, for I/O return errors as negative values
typedef          long  int off_t;       // signed offset
typedef unsigned short int mode_t;      // file mode (dir/file), permissions etc
typedef unsigned long  int uid_t;       // user id
typedef unsigned long  int gid_t;       // group id
typedef          long  int pid_t;       // prod id. Signed allows for special in wait() etc
typedef          long  int tid_t;       // thread id. (tid=pid for main thread)
typedef unsigned long  int phys_addr_t; // physical address. 32 bits -> 4 GB
typedef unsigned long  int virt_addr_t; // virtual address. 32 bits -> 4 GB










// find member offset. GCC has "__builtin_offsetof(type, member)", others do not.
#define offsetof(type, member)     ((size_t)((char *)&(((type *)0)->member) - (char *)0))

// get pointer to the parent structure that contains the pointed member, for mixins
#define container_of(member_ptr, type, member)    (type *)((char *)member_ptr - offsetof(type, member))


#endif
