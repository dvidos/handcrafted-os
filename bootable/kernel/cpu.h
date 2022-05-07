#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>

void cli(void);
void sti(void);
bool interrupts_enabled(void);


void pushcli(void);
void popcli(void);


// we cannot detect if NMI is disabled
void disable_nmi();
void enable_nmi();


// a non-zero means CPUID is supported
extern uint32_t get_cpuid_availability();

#endif
