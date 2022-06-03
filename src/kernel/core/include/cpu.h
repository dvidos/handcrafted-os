#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>
#include <stdbool.h>


// suffix	meaning	number of bits
// b	byte	8
// w	word	16
// l	longword	32
// q	quadword	64

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);

void cli(void);
void sti(void);
bool interrupts_enabled(void);

// these maintain internal level
void pushcli(void);
void popcli(void);

// we cannot detect if NMI is disabled
void disable_nmi();
void enable_nmi();

// a non-zero means CPUID is supported
extern uint32_t get_cpuid_availability();

#endif
