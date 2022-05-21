#include <stdint.h>
#include "cpu.h"
#include "bits.h"

// programmable interrupt controller
// see https://wiki.osdev.org/8259_PIC

#define PIC1_CMD    0x20       // IO base address for master PIC
#define PIC1_DATA   (PIC1_CMD+1)
#define PIC2_CMD    0xA0       // IO base address for slave PIC
#define PIC2_DATA   (PIC2_CMD+1)

#define EOI_CMD     0x20       // End-of-interrupt command code

// reinitialize the PIC controllers, giving them specified vector offsets
// rather than 8h and 70h, as configured by default

#define ICW1_ICW4           0x01    // ICW4 (not) needed
#define ICW1_SINGLE         0x02    // Single (cascade) mode
#define ICW1_INTERVAL4      0x04    // Call address interval 4 (8)
#define ICW1_LEVEL          0x08    // Level triggered (edge) mode
#define ICW1_INIT           0x10    // Initialization - required!

#define ICW4_8086           0x01    // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO           0x02    // Auto (normal) EOI
#define ICW4_BUF_SLAVE      0x08    // Buffered mode/slave
#define ICW4_BUF_MASTER     0x0C    // Buffered mode/master
#define ICW4_SFNM           0x10    // Special fully nested (not)

#define PIC_READ_IRR         0x0a    // OCW3 irq ready next CMD read
#define PIC_READ_ISR         0x0b    // OCW3 irq service next CMD read


static inline void io_wait(void) {
    (void)inb(0);
    // asm volatile ( "jmp 1f\n\t"
    //                "1:jmp 2f\n\t"
    //                "2:" );
}

/*
arguments:
    offset1 - vector offset for master PIC
              vectors on the master become offset1..offset1+7
    offset2 - same for slave PIC: offset2..offset2+7
*/
void pic_remap(int offset1, int offset2)
{
    unsigned char pic1_mask, pic2_mask;
	pic1_mask = inb(PIC1_DATA);                        // save masks
	pic2_mask = inb(PIC2_DATA);

	outb(PIC1_CMD, ICW1_INIT + ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	io_wait();

	outb(PIC2_CMD, ICW1_INIT + ICW1_ICW4);
	io_wait();

	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	io_wait();

	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	io_wait();

	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();

	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();

	outb(PIC1_DATA, ICW4_8086);
	io_wait();

	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	outb(PIC1_DATA, pic1_mask);   // restore saved masks.
	outb(PIC2_DATA, pic2_mask);
}

void pic_send_eoi(unsigned char irq) {
	if (irq >= 8)
		outb(PIC2_CMD, EOI_CMD);

	outb(PIC1_CMD, EOI_CMD);
}

/* Helper func */
static uint16_t __pic_get_irq_register(int ocw3) {
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_CMD, ocw3);
    outb(PIC2_CMD, ocw3);
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t pic_get_ieq_request_register(void) {
    return __pic_get_irq_register(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t pic_get_in_service_register(void) {
    return __pic_get_irq_register(PIC_READ_ISR);
}

// setting a mask on an IRQ line will cause the PIC to ingnore that irq_line
// setting a mask on the slave PIC will cause all IRQs from that PIC to be ignored
void irq_set_mask_bit(uint8_t irq_line) {
    uint16_t port;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    uint8_t value = inb(port);
    value = SET_BIT(value, irq_line);
    outb(port, value);
}

void irq_clear_mask_bit(uint8_t irq_line) {
    uint16_t port;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    uint8_t value = inb(port);
    value = CLEAR_BIT(value, irq_line);
    outb(port, value);
}


void init_pic() {

    // instead of IRQs 0..7, to avoid collision with CPU errors (0..x1F),
    // we remap them to 0x20..0x27, hence the 0x20 offset
    // similarly, for the second PIC, we remap them to 0x28..0x2F, hance the 0x28 offset
    pic_remap(0x20, 0x28);

    // start by ignoring all lines
    for (uint8_t i = 0; i < 32; i++)
        irq_set_mask_bit(i);

    irq_clear_mask_bit(0);  // timer
    irq_clear_mask_bit(1);  // keyboard
    irq_clear_mask_bit(2);  // the slave / chained PIC
    irq_clear_mask_bit(8);  // real time clock
}
