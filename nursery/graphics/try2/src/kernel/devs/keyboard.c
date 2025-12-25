#include <stdint.h>
#include "keyboard.h"
#include "../cpu/ports.h"
#include "../cpu/pic.h"
#include "../aux/logger.h"



void keyboard_isr(void) {
    uint8_t scancode = inb(0x60);

    // TEMP: log or store scancode somewhere
    // log_hex(scancode);
    // 0x1E → A pressed
    // 0x9E → A released
    log.debug("keyboard_isr() scancode = 0x%02x", scancode);


    pic_send_eoi(1);
}
