#include <stdint.h>
#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/pic.h"
#include "../concepts/logger.h"
#include "../concepts/events.h"


// -- scan codes temp buffer ----------------------------------------

#define KBD_RING_BUFFER_SIZE 64

static volatile uint8_t mouse_ring_buffer[KBD_RING_BUFFER_SIZE];
static volatile uint8_t mouse_ring_head = 0;
static volatile uint8_t mouse_ring_tail = 0;

static inline void kbd_queue_push(uint8_t scancode) {
    uint8_t next = (mouse_ring_head + 1) % KBD_RING_BUFFER_SIZE;
    if (next != mouse_ring_tail) {
        mouse_ring_buffer[mouse_ring_head] = scancode;
        mouse_ring_head = next;
    }
}

static int kbd_queue_pop(uint8_t *scancode) {
    if (mouse_ring_tail == mouse_ring_head)
        return 0;

    *scancode = mouse_ring_buffer[mouse_ring_tail];
    mouse_ring_tail = (mouse_ring_tail + 1) % KBD_RING_BUFFER_SIZE;
    return 1;
}

// ----------------------------------------------

static int shift_down = 0;
static int ctrl_down  = 0;
static int alt_down   = 0;
static int super_down   = 0;
static int e0_prefix  = 0;

static const char scancode_table[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3',
    [0x05] = '4', [0x06] = '5', [0x07] = '6',
    [0x08] = '7', [0x09] = '8', [0x0A] = '9',
    [0x0B] = '0',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',
    [0x13] = 'r', [0x14] = 't', [0x15] = 'y',
    [0x16] = 'u', [0x17] = 'i', [0x18] = 'o',
    [0x19] = 'p',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm',
    [0x39] = ' ',
};

static char scancode_to_ascii(uint8_t sc, int shift) {
    char c = scancode_table[sc];
    if (!c) return 0;

    if (shift && c >= 'a' && c <= 'z')
        return c - 32;

    return c;
}

void keyboard_process() {
    uint8_t sc;

    while (kbd_queue_pop(&sc)) {
        // log_hex(scancode);
        // 0x1E → A pressed
        // 0x9E → A released

        if (sc == 0xE0) {
            e0_prefix = 1;
            continue;
        }

        int released = sc & 0x80;
        uint8_t code = sc & 0x7F;

        switch (code) {
            case 0x2A:  // LShift
            case 0x36:  // RShift
                shift_down = !released;
                continue;

            case 0x1D:  // Ctrl
                ctrl_down = !released;
                continue;

            case 0x38:  // Alt
                alt_down = !released;
                continue;

            case 0x5B:  // LSuper
            case 0x5C:  // RSuper
                super_down = !released;
                continue;
        }

        // should publish event to the global event queue.
        key_event_t ev;
        ev.type = released ? KEY_UP : KEY_DOWN;
        ev.keycode = code;
        ev.ascii = scancode_to_ascii(code, shift_down);
        ev.modifiers = (shift_down << 0) | (ctrl_down << 1) |  (alt_down << 2) | (super_down << 3);
        enqueue_key_event(&ev);

        e0_prefix = 0;
    }
}

// -------------------------------------------------------------------

void keyboard_isr(void) {
    // keep it as small as possible, don't decode scancodes here
    uint8_t scancode = inb(0x60);
    //log.debug("keyboard_isr() scancode = 0x%02x", scancode);
    kbd_queue_push(scancode);
    pic_send_eoi(1);
}
