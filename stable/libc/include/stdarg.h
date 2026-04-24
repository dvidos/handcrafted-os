#ifndef _STDARG_H
#define _STDARG_H

// Type for traversing argument list
typedef __builtin_va_list va_list;

// Macros for argument list traversal
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)
#define va_copy(dest, src)  __builtin_va_copy(dest, src)

#endif // _STDARG_H