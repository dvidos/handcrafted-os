#include "../include/ctypes.h"
#include "../arch/cpu.h"
#include "../klib/string.h"
#include "../utils/assert.h"



#define COM1_PORT 0x3f8          // COM1
#define RING_BUFFER_SIZE  128

typedef struct {
    uint16_t base_port;
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} serial_t;

serial_t ports[] = {
    (serial_t){
        .base_port = 0x3f8  // com1
    },
    (serial_t){
        .base_port = 0x2f8  // com2
    }
};


int init_serial_port(int port_idx) {
    ASSERT(port_idx >= 0 && port_idx < 2);
    serial_t *p = &ports[port_idx];

    outb(p->base_port + 1, 0x00);    // Disable all interrupts
    outb(p->base_port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(p->base_port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(p->base_port + 1, 0x00);    //                  (hi byte)
    outb(p->base_port + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(p->base_port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(p->base_port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(p->base_port + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(p->base_port + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if(inb(p->base_port + 0) != 0xAE) {
        return 1;
    }

    outb(p->base_port + 1, 0x01); // Enable "Received Data Available" interrupt

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(p->base_port + 4, 0x0F);
    return 0;
}

static int is_transmit_empty(int port_idx) {
    ASSERT(port_idx >= 0 && port_idx < 2);
    serial_t *p = &ports[port_idx];

    return inb(p->base_port + 5) & 0x20;
}
 
void serial_putchar(int port_idx, char a) {
    if (port_idx < 0 || port_idx > 1)
        return;
    serial_t *p = &ports[port_idx];

    while (is_transmit_empty(port_idx) == 0);
 
    outb(p->base_port, a);
}

void serial_write(int port_idx, const char *str) {
    while (*str != '\0')
        serial_putchar(port_idx, *str++);
}

void serial_enqueue_char(int port_idx, uint8_t c) {
    ASSERT(port_idx >= 0 && port_idx < 2);
    serial_t *p = &ports[port_idx];

    uint32_t next = (p->head + 1) % RING_BUFFER_SIZE;
    if (next != p->tail) { // Don't overwrite if full
        p->buffer[p->head] = c;
        p->head = next;
    }
}

int serial_dequeue_char(int port_idx) {
    ASSERT(port_idx >= 0 && port_idx < 2);
    serial_t *p = &ports[port_idx];

    if (p->head == p->tail)
        return -1; // Buffer empty
    
    uint8_t c = p->buffer[p->tail];
    p->tail = (p->tail + 1) % RING_BUFFER_SIZE;
    return (int)c;
}

void serial_interrupt_handler(int port_idx) {
    // this would be called by the low-level assembly interrupt stub
    ASSERT(port_idx >= 0 && port_idx < 2);
    serial_t *p = &ports[port_idx];
    
    // Check if Data Ready bit (0x01) is set in LSR
    while (inb(p->base_port + 5) & 0x01) {
        uint8_t c = inb(p->base_port);
        serial_enqueue_char(port_idx, c);
    }
    
    // Send EOI (End of Interrupt) to the PIC (for either port)
    outb(0x20, 0x20); 
}

char serial_wait_getc(int port_idx) {
    int c;
    while ((c = serial_dequeue_char(port_idx)) == -1)
        asm("hlt");
    return (char)c;
}

void serial_wait_gets(int port_idx, char *buf, int limit) {
    int i = 0;
    while (i < limit - 1) {
        char c = serial_wait_getc(port_idx);
        
        if (c == '\r' || c == '\n') {
            serial_write(port_idx, "\r\n");
            break;
        }
        
        // Echo back to user and store
        serial_putchar(port_idx, c);
        buf[i++] = c;
    }
    buf[i] = '\0';
}

void serial_panic_writer(const char *str) {
    while (*str != '\0') {
        serial_putchar(0, *str++);
    }
}

void serial_log_appender(void *context, const char *str) {
    // assuming context is the serial port index
    int port_idx = (int)context;
    serial_write(port_idx, str);
}
