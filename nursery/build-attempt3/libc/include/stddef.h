#ifndef _STDDEF_H
#define _STDDEF_H

#include "sys/types.h" // For size_t, ptrdiff_t

// NULL is a common macro for a null pointer constant.
// It can be defined as 0, 0L, or (void*)0. (void*)0 is generally preferred in C.
#ifndef NULL
#define NULL ((void*)0)
#endif

// These are commonly defined in stddef.h
typedef unsigned long size_t;
typedef long ptrdiff_t;

// offsetof macro: Calculates the offset of a member within a structure.
// This is a common implementation, though compiler intrinsics may be used.
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif // _STDDEF_H