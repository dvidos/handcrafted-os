#include "mouse.h"
#include "../cpu/ports.h"
#include "../cpu/pic.h"
#include "../concepts/logger.h"
#include "../concepts/events.h"

// -----------------------------------------------------------

#define MOUSE_RING_BUFFER_SIZE 256

static volatile uint8_t mouse_ring_buffer[MOUSE_RING_BUFFER_SIZE];
static volatile uint8_t mouse_ring_head = 0;
static volatile uint8_t mouse_ring_tail = 0;

static inline void mouse_queue_push(uint8_t byte) {
    uint8_t next = (mouse_ring_head + 1) % MOUSE_RING_BUFFER_SIZE;
    if (next != mouse_ring_tail) {
        mouse_ring_buffer[mouse_ring_head] = byte;
        mouse_ring_head = next;
    }
}

int mouse_queue_pop(uint8_t *byte) {
    if (mouse_ring_tail == mouse_ring_head)
        return 0;

    *byte = mouse_ring_buffer[mouse_ring_tail];
    mouse_ring_tail = (mouse_ring_tail + 1) % MOUSE_RING_BUFFER_SIZE;
    return 1;
}

// ----------------------------------------------

static mouse_xy_retrieval_func *mouse_retrieve_xy_func;
static mouse_xy_update_func *mouse_changed_notifier_func;
#define MOUSE_PACKET_SIZE_BYTES  4
static int8_t mouse_packet[MOUSE_PACKET_SIZE_BYTES];
static uint8_t packet_index = 0;
static int mouse_x = 0;
static int mouse_y = 0;
static uint8_t mouse_buttons = 0;




// ----------------------------------------------

static inline void ps2_wait_read(void) { while (!(inb(0x64) & 1)); }
static inline void ps2_wait_write(void) { while (inb(0x64) & 2); }
void mouse_write(uint8_t val) { ps2_wait_write(); outb(0x64, 0xD4); ps2_wait_write(); outb(0x60, val); }
uint8_t mouse_read(void) { ps2_wait_read(); return inb(0x60); }


void initialize_mouse(mouse_xy_retrieval_func *retrieve, mouse_xy_update_func *update) {

    // Enable auxiliary device
    ps2_wait_write();
    outb(0x64, 0xA8);

    // Enable IRQ12
    ps2_wait_write();
    outb(0x64, 0x20);
    ps2_wait_read();
    uint8_t status = inb(0x60);
    status |= 0x02;
    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, status);

    // reset to defaults
    mouse_write(0xF6);
    mouse_read(); // ACK

    // --- 4. Enable IntelliMouse wheel (4-byte packets) ---
    // Sequence required: 200, 100, 80 sample rates
    mouse_write(0xF3);  // Set sample rate
    mouse_read();       // ACK
    mouse_write(200);
    mouse_read();       // ACK
    mouse_write(0xF3);
    mouse_read();
    mouse_write(100);
    mouse_read();
    mouse_write(0xF3);
    mouse_read();
    mouse_write(80);
    mouse_read();
    // After this, mouse reports 4-byte packets (last byte = wheel)

    // Enable packet streaming
    mouse_write(0xF4);
    mouse_read(); // ACK

    mouse_retrieve_xy_func = retrieve;
    mouse_changed_notifier_func = update;
}

// ------------------------------------------------------------------------


void decode_mouse_packet() {
    /*  mouse packet:
        byte 0:
            bit 0: left button
            bit 1: right button
            bit 2: middle button
            bit 3: always 1  (flags packet start)
            bit 4: X sign
            bit 5: Y sign
            bit 6: X overflow
            bit 7: Y overflow
        byte 1: X movement (signed)
        byte 2: Y movement (signed, inverted)
    */

    int dx = (int8_t)mouse_packet[1];
    int dy = -(int8_t)mouse_packet[2]; // ps/2 y is inverted

    if (mouse_retrieve_xy_func != 0)
        mouse_retrieve_xy_func(&mouse_x, &mouse_y);
    mouse_x += dx;
    mouse_y += dy;

    // maybe we should convert this from buttons=X to actual events MOUSE_LBUTTON_DOWN/UP, wheel events etc
    mouse_event_t ev;
    ev.dx = dx;
    ev.dy = dy;
    ev.x = mouse_x;
    ev.y = mouse_y;
    ev.buttons = mouse_packet[0] & 0x07;
    ev.wheel = (int8_t)mouse_packet[3];
    // log.debug("mouse x=%d, y=%d, buttons=%d", mouse_x, mouse_y, mouse_buttons);
    enqueue_mouse_event(&ev);  

    if (mouse_changed_notifier_func != 0)
        mouse_changed_notifier_func(mouse_x, mouse_y);
}

void mouse_process() {
    uint8_t byte;

    while (mouse_queue_pop(&byte)) {
        // First byte must have bit 3 set, resync
        if (packet_index == 0 && !(byte & 0x08))
            continue;

        mouse_packet[packet_index++] = byte;
        if (packet_index == MOUSE_PACKET_SIZE_BYTES) {
            decode_mouse_packet();
            packet_index = 0;
        }
    }
}

// ------------------------------------------------------

void mouse_isr(void) {
    // keep it as small as possible, don't decode scancodes here
    uint8_t data = inb(0x60);
    // log.debug("mouse_isr() data = 0x%02x", data);
    mouse_queue_push(data);
    pic_send_eoi(12);
}

