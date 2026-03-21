#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "../include/ctypes.h"
#include "../arch/idt.h"

struct syscall_stack
{
    union {
        struct {
            uint32_t original_ds;
            uint32_t arg5, arg4, arg3, arg2, arg1, sysno;
        } passed;
        struct {
            uint32_t dword[12];
        } uniform;
    };
};

// int isr_syscall(struct syscall_stack stack);
int isr_syscall(trap_frame_t *regs);

#endif
