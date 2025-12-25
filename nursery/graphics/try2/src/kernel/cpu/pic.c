#include "ports.h"

// programmable interrupt controller


void pic_remap_irqs(void) {

    // remapping because by default IRQs overlap CPU exceptions.
    // Master PIC: 0x20 (cmd), 0x21 (data)
    // Slave  PIC: 0xA0 (cmd), 0xA1 (data)

    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20); // master offset
    outb(0xA1, 0x28); // slave offset

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, a1);
    outb(0xA1, a2);
}

void pic_enable_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t  mask = inb(port);
    mask &= ~(1 << (irq % 8));
    outb(port, mask);
}

void pic_disable_all_irqs(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void pic_send_eoi(uint8_t irq) {  // Call this at the end of every IRQ handler
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
