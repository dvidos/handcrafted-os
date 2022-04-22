#include <stdint.h>
#include "ports.h"
#include "bits.h"


#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

#define PIC_EOI		0x20		/* End-of-interrupt command code */

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */

#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

#define PIC_READ_IRR                0x0a    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR                0x0b    /* OCW3 irq service next CMD read */


static inline void io_wait(void) {
    /* TODO: This is probably fragile. */
    asm volatile ( "jmp 1f\n\t"
                   "1:jmp 2f\n\t"
                   "2:" );
}

/*
arguments:
	offset1 - vector offset for master PIC
		vectors on the master become offset1..offset1+7
	offset2 - same for slave PIC: offset2..offset2+7
*/
void pic_remap(int offset1, int offset2)
{
    unsigned char a1, a2;
	a1 = port_byte_in(PIC1_DATA);                        // save masks
	a2 = port_byte_in(PIC2_DATA);

	port_byte_out(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	io_wait();
	port_byte_out(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
	io_wait();
	port_byte_out(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	io_wait();
	port_byte_out(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	io_wait();
	port_byte_out(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	port_byte_out(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();

	port_byte_out(PIC1_DATA, ICW4_8086);
	io_wait();
	port_byte_out(PIC2_DATA, ICW4_8086);
	io_wait();

	port_byte_out(PIC1_DATA, a1);   // restore saved masks.
	port_byte_out(PIC2_DATA, a2);
}

void pic_send_eoi(unsigned char irq) {
	if (irq >= 8)
		port_byte_out(PIC2_COMMAND,PIC_EOI);

	port_byte_out(PIC1_COMMAND,PIC_EOI);
}

/* Helper func */
static uint16_t __pic_get_irq_reg(int ocw3) {
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    port_byte_out(PIC1_COMMAND, ocw3);
    port_byte_out(PIC2_COMMAND, ocw3);
    return (port_byte_in(PIC2_COMMAND) << 8) | port_byte_in(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t pic_get_irr(void) {
    return __pic_get_irq_reg(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t pic_get_isr(void) {
    return __pic_get_irq_reg(PIC_READ_ISR);
}

void irq_set_mask(uint8_t irq_line) {
    uint16_t port;
    uint8_t value;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = SET_BIT(port_byte_in(port), irq_line);
    port_byte_out(port, value);
}

void irq_clear_mask(uint8_t irq_line) {
    uint16_t port;
    uint8_t value;

    if(irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }
    value = CLEAR_BIT(port_byte_in(port), irq_line);
    port_byte_out(port, value);
}


void init_pic() {
    pic_remap(0x20, 0x28);
    for (uint8_t i = 0; i < 32; i++)
        irq_set_mask(i);
    irq_clear_mask(0x00);
    irq_clear_mask(0x01);
}
