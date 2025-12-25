#include "idt.h"
#include "../memory/string.h"

// interrupt descriptor table

/*
    To create a new interrupt handler:
    - create a C "void handler()" method, that calls pic_send_eoi() at end.
    - create a stub in the assembly file, that calls the C handler and does "iret"
    - add the stub in the table, in the initialize_idt() function
    - unmask this interrupt when you want to start using it (e.g. initialiaze the relevant device)
*/

#define IDT_ENTRIES 256

struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr   idt_descriptor;

void idt_set_gate(uint8_t vec, uint32_t handler) {
    idt[vec].offset_low  = handler & 0xFFFF;
    idt[vec].selector    = 0x08;      // kernel code segment (see stage2.asm)
    idt[vec].zero        = 0;
    idt[vec].type_attr   = 0x8E;      // present, ring 0, 32-bit interrupt gate
    idt[vec].offset_high = handler >> 16;
}


extern void lidt_asm(void*);
extern void irq0_stub_asm(void);
extern void irq1_stub_asm(void);
extern void irq12_stub_asm(void);
extern void exception_stub_asm(void);


void initialize_idt(void) {
    memset(idt, 0, sizeof(idt));

    // just to avoid tripple faults
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, (uint32_t)exception_stub_asm);

    // add what irqs we want to be called
    idt_set_gate(0x20, (uint32_t)irq0_stub_asm); // calls timer_isr
    idt_set_gate(0x21, (uint32_t)irq1_stub_asm); // calls keyboard_isr
    idt_set_gate(0x2C, (uint32_t)irq12_stub_asm); // calls mouse_isr
    
    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (uint32_t)&idt;
    lidt_asm(&idt_descriptor);
}
