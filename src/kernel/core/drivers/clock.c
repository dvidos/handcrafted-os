#include <stdbool.h>
#include <stdint.h>
#include "../cpu.h"
#include "../drivers/screen.h"
#include "../drivers/clock.h"
#include "../lock.h"
#include "../cpu.h"
#include "../idt.h"


// mostly following https://wiki.osdev.org/CMOS#Getting_Current_Date_and_Time_from_RTC
// also https://web.archive.org/web/20150514082645/http://www.nondot.org/sabre/os/files/MiscHW/RealtimeClockFAQ.txt
// also http://bos.asmhackers.net/docs/timer/docs/cmos.pdf
// dow is unreliable (without a centrury one cannot be certain)
// The CENTURY field (108 bytes in) of the Fixed ACPI Description Table will contain an offset into the CMOS which you can use to create the correct year. 

// in my Linux laptop, the clock was set to UTC, e.g. 4 hours away from EDT or 5 hours away from EST (UTC has no DST)
// also time zone is something we cannot know.

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT    0x71

#define BCD_TO_DECIMAL(value)   (((((value) & 0xF0) >> 4) * 10) + ((value) & 0x0F))
#define DECIMAL_TO_BCD(value)   ((((value) / 10) << 4) + ((value) % 10))

static uint8_t get_clock_register(uint16_t reg);
static void set_clock_register(uint16_t reg, uint8_t value);



static inline uint8_t get_clock_register(uint16_t reg) {
    // the 7th bit of port 0x70 is the NMI mask value, we should avoid changing it
    // but the port is write only, it always returns 0xFF...
    // also, a write or read on port 0x71 is always expected after a write on port 0x70
    outb(CMOS_ADDRESS_PORT, reg);
    return inb(CMOS_DATA_PORT);
}

static inline void set_clock_register(uint16_t reg, uint8_t value) {
    // the 7th bit of port 0x70 is the NMI mask value, we should avoid changing it
    // but the port is write only, it always returns 0xFF...
    // also, a write or read on port 0x71 is always expected after a write on port 0x70
    outb(CMOS_ADDRESS_PORT, reg);
    outb(CMOS_DATA_PORT, value);
}

static inline bool update_in_progress_flag() {
    return get_clock_register(0x0A) & 0x80;
}

// this will overflow every 136 years
static volatile uint32_t seconds_since_boot = 0;
static volatile uint8_t half_second = 0;
static volatile lock_t clock_writing_lock;


void init_real_time_clock(uint8_t interrupt_divisor) {
    acquire(&clock_writing_lock);
    pushcli();

    // the highest bit in address port is the NMI mask
    // we can turn NMI off (by turning on bit 7), but we cannot detect if it was originally on.
    // we have to keep enabling bit 7 in our addressing here

    // it is imperative that we disable NMI during setup, otherwise
    // an interrupt may leave the clock in a bad state
    // see https://www.compuphase.com/int70.txt

    // Interrupt divider is four bits how to divide the 32768 Hz crystal of the RTC
    // rate in Hz: 0001=256, 0010=128, 0011=8192, 0100=4096, 0101=2048, 0110=1024, 0111=512, 1000=256, 1001=128, 1010=64, 1011=32, 1100=16, 1101=8, 1110=4, 1111=2
    // (0x8x for turning off NMI)
    uint8_t reg_a = get_clock_register(0x8A);
    set_clock_register(0x8A, (reg_a & 0xF0) | (interrupt_divisor & 0x0F));
    
    // turn on the periodic interrupt (bit 6)
    uint8_t reg_b = get_clock_register(0x8B);
    set_clock_register(0x8B, reg_b | 0x40);
    
    popcli();
    release(&clock_writing_lock);
}

void real_time_clock_interrupt_interrupt_handler(registers_t *regs) {
    uint8_t type_of_interrupt = get_clock_register(0x0C);
    // Status Register C – Type of Interupt
    // Bit 7 – Any
    // Bit 6 – Periodic
    // Bit 5 – Alarm
    // Bit 4 – Update Ended    
    // printf("(tick)", type_of_interrupt);

    if (type_of_interrupt & 0x40) {
        // we mostly would like an interrupt per second, but divider of 0xF gives us 2 Hz.
        // so we lose half cycles here.
        half_second = !half_second;
        if (!half_second) {
            seconds_since_boot++;
        }
    }

    (void)regs;
}

uint32_t get_uptime_in_seconds() {
    return seconds_since_boot;
}

void get_real_time_clock(clock_time_t *p) {
    // must make sure no update is in progress, otherwise we may get bogus values
    // we shall read two consecutive times, to make sure we get it right
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t days;
    uint8_t dow;
    uint8_t months;
    uint8_t years;
    uint8_t centuries;

    uint8_t last_seconds;
    uint8_t last_minutes;
    uint8_t last_hours;
    uint8_t last_days;
    uint8_t last_dow;
    uint8_t last_months;
    uint8_t last_years;
    uint8_t last_centuries;

    // wait for no update in progress
    while (update_in_progress_flag())
        ;

    seconds = get_clock_register(0x00);
    minutes = get_clock_register(0x02);
    hours = get_clock_register(0x04);
    dow = get_clock_register(0x06);
    days = get_clock_register(0x07);
    months = get_clock_register(0x08);
    years = get_clock_register(0x09);
    centuries = get_clock_register(0x32);

    // repeat reads to make sure no update was in progress
    while (true) {
        last_seconds = seconds;
        last_minutes = minutes;
        last_hours = hours;
        last_dow = dow;
        last_days = days;
        last_months = months;
        last_years = years;
        last_centuries = centuries;

        // wait for no update in progress
        while (update_in_progress_flag())
            ;

        seconds = get_clock_register(0x00);
        minutes = get_clock_register(0x02);
        hours = get_clock_register(0x04);
        dow = get_clock_register(0x06);
        days = get_clock_register(0x07);
        months = get_clock_register(0x08);
        years = get_clock_register(0x09);
        centuries = get_clock_register(0x32);

        if (seconds == last_seconds 
            && minutes == last_minutes
            && hours == last_hours
            && dow == last_dow
            && days == last_days
            && months == last_months
            && years == last_years
            && centuries == last_centuries)
            break;
    }

    // uint8_t register_a = get_clock_register(0x0A);
    // Status Register A
    // Bit 7 – Update in Progress (Read)
    // Bits 0 to 3 – Periodic Interupt Rate Hz (0001=256, 0010=128, 0011=8192, 0100=4096, 0101=2048, 0110=1024, 0111=512, 1000=256, 1001=128, 1010=64, 1011=32, 1100=16, 1101=8, 1110=4, 1111=2)

    uint8_t register_b = get_clock_register(0x0B);
    // Status Register B
    // Bit 7 – Abort Update(allow access to clock data)
    // Bit 6 – Enable Periodic Interupt
    // Bit 5 – Enable Alarm Interupt
    // Bit 4 – Enable Update Ended Interupt
    // Bit 2 – Clock Data Type (0=BCD, 1=Binary, BIOS default=0)
    // Bit 1 – Hour Data Type (0=12 hour, 1=24 hour, BIOS default=1)
    // Bit 0 – Daylight Savings Enable

    // uint8_t register_c = get_clock_register(0x0c);
    // Status Register C – Type of Interupt
    // Bit 7 – Any
    // Bit 6 – Periodic
    // Bit 5 – Alarm
    // Bit 4 – Update Ended    

    // uint8_t register_d = get_clock_register(0x0d);
    // POST Diagnostics Status
    // Bit 7 – Clock Lost Power
    // Bit 6 – CMOS Bad Checksum
    // Bit 5 – Invalid Configuration @ POST
    // Bit 4 – Memory Size Compare Error
    // Bit 3 – Disk or Controller Error
    // Bit 2 – Invalid Time or Data (32nd)


    // printf("RTC registers: a:%02x  b:%02x\n", register_a, register_b);
    // printf("RTC values:    s:%02x  m:%02x  h:%02x  w:%02x  d:%02x  m:%02x  y:%02x  c:%02x\n", seconds, minutes, hours, dow, days, months, years, centuries);

    // bit 3 in register b tells us if values are already in decimal. 
    bool is_bcd_format = (register_b & 0x04) == 0;
    bool is_12h_format = (register_b & 0x02) == 0;

    // in case we are in 12h mode, we need to detect PM bit, set in either encoding
    bool is_pm = hours & 0x80;
    hours &= ~(0x80);

    // convert values as needed
    if (is_bcd_format) {
        p->seconds = BCD_TO_DECIMAL(seconds);
        p->minutes = BCD_TO_DECIMAL(minutes);
        p->hours = BCD_TO_DECIMAL(hours);
        p->dow = BCD_TO_DECIMAL(dow);
        p->days = BCD_TO_DECIMAL(days);
        p->months = BCD_TO_DECIMAL(months);
        p->years = (uint16_t)BCD_TO_DECIMAL(centuries) * 100 + (uint16_t)BCD_TO_DECIMAL(years);
    } else {
        p->seconds = seconds;
        p->minutes = minutes;
        p->hours = hours;
        p->dow = dow;
        p->days = days;
        p->months = months;
        p->years = (uint16_t)centuries * 100 + (uint16_t)years;
    }

    // convert hour to 24h format
    if (is_12h_format && is_pm)
        p->hours += 12;
}
