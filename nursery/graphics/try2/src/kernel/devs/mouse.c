#include "mouse.h"
#include "../cpu/ports.h"
#include "../cpu/pic.h"
#include "../aux/logger.h"


static inline void ps2_wait_read(void) {
    while (!(inb(0x64) & 1));
}

static inline void ps2_wait_write(void) {
    while (inb(0x64) & 2);
}

void mouse_write(uint8_t val) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, val);
}

uint8_t mouse_read(void) {
    ps2_wait_read();
    return inb(0x60);
}

void initialize_mouse(void) {

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

    // Set defaults
    mouse_write(0xF6);
    mouse_read(); // ACK

    // Enable packet streaming
    mouse_write(0xF4);
    mouse_read(); // ACK
}

static int8_t mouse_packet[3];
static uint8_t packet_cycle = 0;
static int mouse_x = 0;
static int mouse_y = 0;
static uint8_t mouse_buttons = 0;

void mouse_handle_packet(void) {
    int dx = (int8_t)mouse_packet[1];
    int dy = (int8_t)mouse_packet[2];

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
    dy = -dy; // PS/2 Y is inverted

    mouse_x += dx;
    mouse_y += dy;

    //clamp_to_screen(&mouse_x, &mouse_y);

    mouse_buttons = mouse_packet[0] & 0x07;
    log.info("mouse x=%d, y=%d, buttons=%d", mouse_x, mouse_y, mouse_buttons);
}

// ------------------------------------------------------


void mouse_isr(void) {
    uint8_t data = inb(0x60);

    switch (packet_cycle) {
        case 0:
            if (!(data & 0x08)) break; // resync
            mouse_packet[0] = data;
            packet_cycle = 1;
            break;
        case 1:
            mouse_packet[1] = data;
            packet_cycle = 2;
            break;
        case 2:
            mouse_packet[2] = data;
            packet_cycle = 0;
            mouse_handle_packet();
            break;
    }

    pic_send_eoi(12);
}

