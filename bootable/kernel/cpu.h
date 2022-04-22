#ifndef _CPU_H
#define _CPU_H

#define disable_interrupts()   asm volatile ("cli")
#define enable_interrupts()    asm volatile ("sti")


#endif
