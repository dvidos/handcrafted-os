#pragma once


#define DUMP_CPU_INFO()    \
    uint32_t current_esp;   \
    uint32_t current_eip;   \
    uint32_t stack_top[4];  \
    \
    asm volatile("mov %%esp, %0" : "=r"(current_esp));  \
    asm volatile("call 1f\n" "1: pop %0" : "=r"(current_eip));  \
    \
    uint32_t *ptr = (uint32_t*)current_esp; \
    stack_top[0] = ptr[0]; \
    stack_top[1] = ptr[1]; \
    stack_top[2] = ptr[2]; \
    stack_top[3] = ptr[3]; \
    \
    log_info("PID %d | EIP: 0x%08x | ESP: 0x%08x", running_process()->pid, current_eip, current_esp); \
    log_info("Stack Peek: [0]:0x%08x [1]:0x%08x [2]:0x%08x [3]:0x%08x", stack_top[0], stack_top[1], stack_top[2], stack_top[3]); \


void log_interrupt_status(const char* location);
