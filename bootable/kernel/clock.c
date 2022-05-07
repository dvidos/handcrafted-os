#include <stdbool.h>
#include <stdint.h>
#include "ports.h"


// mostly following https://wiki.osdev.org/CMOS#Getting_Current_Date_and_Time_from_RTC
// dow is unreliable (without a centrury one cannot be certain)
// The CENTURY field (108 bytes in) of the Fixed ACPI Description Table will contain an offset into the CMOS which you can use to create the correct year. 

// in my Linux laptop, the clock was set to UTC, e.g. 4 hours away from EDT or 5 hours away from EST (UTC has no DST)
// also time zone is something we cannot know.


#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

#define BCD_TO_DECIMAL(value)   (((((value) & 0xF0) >> 4) * 10) + ((value) & 0x0F))
#define DECIMAL_TO_BCD(value)   ((((value) / 10) << 4) + ((value) % 10))


struct clock_time {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t days;
    uint8_t dow;
    uint8_t months;
    uint8_t years;
    uint8_t centuries;
};
typedef struct clock_time clock_time_t;


static inline uint8_t get_clock_register(uint16_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static inline void set_clock_register(uint16_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, reg);
    outb(CMOS_DATA, value);
}

static inline bool update_in_progress_flag() {
    return get_clock_register(0x0A) & 0x80;
}

void get_real_time_clock(clock_time_t *p) {
    // must make sure no update is in progress, otherwise we may get bogus values
    // we shall read two consecutive times, to make sure we get it right

    uint8_t last_seconds;
    uint8_t last_minutes;
    uint8_t last_hours;
    uint8_t last_days;
    uint8_t last_dow;
    uint8_t last_months;
    uint8_t last_years;

    // wait for no update in progress
    while (update_in_progress_flag())
        ;

    // registers 1, 3, 5 are the seconds, minutes and hours alarm values
    // 6 is day of week

    p->seconds = get_clock_register(0x00);
    p->minutes = get_clock_register(0x02);
    p->hours = get_clock_register(0x04);
    p->dow = get_clock_register(0x06);
    p->days = get_clock_register(0x07);
    p->months = get_clock_register(0x08);
    p->years = get_clock_register(0x09);
    // centuries = get_clock_register(0xFF);  // may need to parse ACPI table to see if available

    // repeat reads to make sure no update was in progress
    while (true) {
        last_seconds = p->seconds;
        last_minutes = p->minutes;
        last_hours = p->hours;
        last_dow = p->dow;
        last_days = p->days;
        last_months = p->months;
        last_years = p->years;

        // wait for no update in progress
        while (update_in_progress_flag())
            ;

        p->seconds = get_clock_register(0x00);
        p->minutes = get_clock_register(0x02);
        p->hours = get_clock_register(0x04);
        p->dow = get_clock_register(0x06);
        p->days = get_clock_register(0x07);
        p->months = get_clock_register(0x08);
        p->years = get_clock_register(0x09);
        // centuries = get_clock_register(0xFF);  // may need to parse ACPI table to see if available

        if (p->seconds == last_seconds 
            && p->minutes == last_minutes
            && p->hours == last_hours
            && p->dow == last_dow
            && p->days == last_days
            && p->months == last_months
            && p->years == last_years)
            break;
    }

    uint8_t register_a = get_clock_register(0x0A);
    uint8_t register_b = get_clock_register(0x0B);
    uint8_t register_c = get_clock_register(0x0C);
    uint8_t register_d = get_clock_register(0x0D);

    // printf("RTC registers: a:%02x  b:%02x  c:%02x  d:%02x\n", register_a, register_b, register_c, register_d);
    // printf("RTC values:    0:%02x  2:%02x  4:%02x  6:%02x  7:%02x  8:%02x  9:%02x\n", p->seconds, p->minutes, p->hours, p->dow, p->days, p->months, p->years);

    // bit 3 in register b tells us if values are already in decimal. 
    // otherwise, convert them
    if ((register_b & 0x04) == 0) {
        p->seconds = BCD_TO_DECIMAL(p->seconds);
        p->minutes = BCD_TO_DECIMAL(p->minutes);
        p->hours = BCD_TO_DECIMAL(p->hours);
        p->dow = BCD_TO_DECIMAL(p->dow);
        p->days = BCD_TO_DECIMAL(p->days);
        p->months = BCD_TO_DECIMAL(p->months);
        p->years = BCD_TO_DECIMAL(p->years);
    }
}

void set_real_time_clock(clock_time_t *p) {
    // not yet implemented
}

