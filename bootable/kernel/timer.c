#include <stdint.h>
#include "ports.h"
#include "idt.h"
#include "screen.h"



// ports for setting up the PIT
#define CHANNEL_0_DATA_PORT   0x40
#define CHANNEL_1_DATA_PORT   0x41
#define CHANNEL_2_DATA_PORT   0x42
#define MODE_COMMAND_PORT     0x43

// commands for the command ports
// bits 7,6
#define SELECT_CHANNEL_0                   (0x0 << 6)
#define SELECT_CHANNEL_1                   (0x1 << 6)
#define SELECT_CHANNEL_2                   (0x2 << 6)
#define READ_BACK                          (0x3 << 6)
// bits 5,4
#define LATCH_COUNT_VALUE                  (0x0 << 4)
#define ACCESS_LOW_BYTE                    (0x1 << 4)
#define ACCESS_HIGH_BYTE                   (0x2 << 4)
#define ACCESS_LO_HI_BYTE                  (0x3 << 4)
// bits 3,2,1
#define MODE_INTERRUPT_ON_TERMINAL_COUNT   (0x0 << 1)
#define MODE_RETRIGGERABLE_ONE_SHOT        (0x1 << 1)
#define MODE_RATE_GENERATOR                (0x2 << 1)
#define MODE_SQUARE_WAVE_GENERATOR         (0x3 << 1)
#define MODE_SOFTWARE_TRIGGERED_STROBE     (0x4 << 1)
#define MODE_HARDWARE_TRIGGERED_STROBE     (0x5 << 1)
#define MODE_RATE_GENERATOR_2              (0x6 << 1)
#define MODE_SQUARE_WAVE_GENERATOR_2       (0x7 << 1)
// bit 0
#define DIGIT_16_BIT_BINARY                (0x0)
#define DIGIT_FOUR_DIGIT_BCD               (0x1)

// this will take thousands of years to overflow
volatile uint64_t ticks = 0;



void init_timer(uint32_t frequency) {

    // we are using the Programmable Interval Timer (PIT)
    // it's working at 1.193182 MHz and we give it a prescaler value
    // when the prescaler reaches zero, it triggers IRQ zero
    // see https://wiki.osdev.org/Programmable_Interval_Timer

    uint32_t divisor = 1193180 / frequency;
    uint8_t mode = ACCESS_LO_HI_BYTE | MODE_SQUARE_WAVE_GENERATOR;
    outb(MODE_COMMAND_PORT, mode);
    outb(CHANNEL_0_DATA_PORT, (uint8_t)(divisor & 0xFF));
    outb(CHANNEL_0_DATA_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler(registers_t *regs) {
    ticks++;
    ((void)regs);
}

uint64_t timer_get_uptime_ticks() {
    return ticks;
}

void pause(int milliseconds) {
    uint64_t target = ticks + milliseconds;
    while (ticks < target);
}
