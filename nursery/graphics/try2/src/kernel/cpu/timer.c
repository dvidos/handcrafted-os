#include "pic.h"
#include <stdint.h>


volatile uint32_t timer_ticks = 0;

uint32_t get_timer_ticks() {
    return timer_ticks;
}

void timer_isr(void) {
    timer_ticks++;
    pic_send_eoi(0);
}

