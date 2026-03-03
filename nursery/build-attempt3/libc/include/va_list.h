#ifndef _VA_LIST_H
#define _VA_LIST_H

typedef __builtin_va_list va_list;

#define va_start(list_var, anchor_var)   __builtin_va_start(list_var, anchor_var)
#define va_arg(list_var, arg_type)     __builtin_va_arg(list_var, arg_type)
#define va_end(list_var)       __builtin_va_end(list_var)


#endif
