#ifndef _TRAPFRAME_H
#define _TRAPFRAME_H

#include "../include/ctypes.h"


/**
 * We want to have the same trapframe, when we are
 * (a) called via syscall, (b) switching due to timer
 * 
 * Assembly will push things in order, and then push ESP,
 * so that the C function receives a pointer to that
 * sequence of values, hence the struct below.
 * 
 * What is pushed first is towards the bottom of the struct
 * and what is pushed last is towards the top
 */
typedef struct trapframe {
    // pushed manually (reverse order of pushes)
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // pushad order
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;     // for syscall, modify this to return value to caller (e.g. fork())

    // pushed by CPU automatically
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;

    // only if ring switch
    uint32_t user_esp;
    uint32_t ss;

} __attribute__((packed)) trapframe_t;




#endif
