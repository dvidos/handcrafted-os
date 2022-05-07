#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>

void cli(void);
void sti(void);

void pushcli(void);
void popcli(void);




// a non-zero means CPUID is supported
extern uint32_t get_cpuid_availability();

#endif
