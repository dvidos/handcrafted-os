#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>

#define disable_interrupts()   asm volatile ("cli")
#define enable_interrupts()    asm volatile ("sti")

// a non-zero means CPUID is supported
extern uint32_t get_cpuid_availability();

#endif
