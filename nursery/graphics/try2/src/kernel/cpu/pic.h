#pragma once
#include <stdint.h>

// programmable interrupt conbtroller

// 0-15 exceptions, 16-31 interrupts
void pic_remap_irqs(void);
void pic_enable_irq(uint8_t irq);
void pic_disable_all_irqs(void);


// Call this at the end of every IRQ handler
void pic_send_eoi(uint8_t irq);
