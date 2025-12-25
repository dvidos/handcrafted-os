#pragma once
#include <stdint.h>

// interrupt descriptor table

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));


struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


void initialize_idt(void);
void idt_set_gate(uint8_t vec, uint32_t handler);
