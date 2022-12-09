#ifndef _TYPES_H
#define _TYPES_H


#define NULL  0
#define false 0
#define true  1

// for functions returning int
#define ERROR  -1
#define SUCCESS 0

typedef signed char int8;
typedef signed short int16;
typedef signed long int32;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

typedef unsigned char bool;
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

#define KB   1024
#define MB   (1024 * KB)
#define GB   (1024 * MB)

#define freeze()    for(;;) asm("hlt")

typedef __builtin_va_list va_list;

#define va_start(list_var, anchor_var)   __builtin_va_start(list_var, anchor_var)
#define va_arg(list_var, arg_type)     __builtin_va_arg(list_var, arg_type)
#define va_end(list_var)       __builtin_va_end(list_var)



#endif
