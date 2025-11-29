#include <time.h>
#include "clock_device.h"

struct clock_device_data {
    uint32_t preset;
};

static struct clock_device_data clock_device_data = {
    .preset = 0
};

static uint32_t get_rtc_seconds_since_epoch(clock_device *clock) {
    time_t t = time(NULL);   // seconds since 1970-01-01
    if (t < 0) t = 0;        // clamp negative values just in case
    return (uint32_t)t;      // cast to unsigned 32-bit
}

static uint32_t get_fixed_seconds_since_epoch(clock_device *clock) {
    return ((struct clock_device_data *)clock->clock_data)->preset;
}


static clock_device rtc_clock = { 
    .clock_data = &clock_device_data,
    .get_seconds_since_epoch = get_rtc_seconds_since_epoch
};

static clock_device fixed_clock = {
    .clock_data = &clock_device_data,
    .get_seconds_since_epoch = get_fixed_seconds_since_epoch
};

clock_device *new_rtc_based_clock_device() {
    return &rtc_clock;
}

clock_device *new_fixed_clock_device(uint32_t preset_seconds) {
    ((struct clock_device_data *)fixed_clock.clock_data)->preset = preset_seconds;
    return &fixed_clock;
}
