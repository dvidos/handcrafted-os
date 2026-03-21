#ifndef _IDT_H
#define _IDT_H

// things pushed in the isr_stub we have in assembly
// this is passed when isr_handler is called from our assembly stub
typedef struct registers {
   uint32_t ds;   // Data segment selector

   uint32_t edi;
   uint32_t esi;
   uint32_t ebp;
   uint32_t esp_dummy;
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax; // Pushed by pusha.
   
   uint32_t int_no;
   uint32_t err_code;    // Interrupt number and error code (if applicable)
   
   uint32_t eip;
   uint32_t cs;
   uint32_t eflags;
   uint32_t user_esp;
   uint32_t ss; // Pushed by the processor automatically.
} registers_t;



void init_idt(uint16_t code_segment_selector);



#endif
