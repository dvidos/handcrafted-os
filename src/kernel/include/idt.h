#ifndef _IDT_H
#define _IDT_H

// things pushed in the isr_stub we have in assembly
// this is passed when isr_handler is called from our assembly stub
typedef struct registers
{
   uint32_t ds;                  // Data segment selector
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
   uint32_t int_no, err_code;    // Interrupt number and error code (if applicable)
   uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} registers_t;



void init_idt(uint16_t code_segment_selector);



#endif
