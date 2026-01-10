#include "pic.h"
#include "../fundamentals.h"


volatile uint32_t timer_ticks = 0;

uint32_t get_timer_ticks() {
#ifdef HOSTED_ENV
    extern uint32_t SDL_GetTicks();
    return SDL_GetTicks();
#else
    return timer_ticks;
#endif
}

void timer_isr(void) {
    timer_ticks++;
    pic_send_eoi(0);
}

