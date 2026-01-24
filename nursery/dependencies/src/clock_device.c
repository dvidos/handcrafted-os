#include <time.h>
#include "clock_device.h"

struct clock_device_data {
    uint32_t preset_seconds;
    uint32_t preset_msecs;
};

static struct clock_device_data clock_device_data = {
    .preset_seconds = 0
};

static uint32_t get_rtc_seconds_since_epoch(clock_device *clock) {
    time_t t = time(NULL);   // seconds since 1970-01-01
    if (t < 0) t = 0;        // clamp negative values just in case
    return (uint32_t)t;      // cast to unsigned 32-bit
}

static uint32_t get_rtc_msecs_since_boot(clock_device *clock) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

static uint32_t get_fixed_seconds_since_epoch(clock_device *clock) {
    return ((struct clock_device_data *)clock->clock_data)->preset_seconds;
}

static uint32_t get_fixed_msecs_since_boot(clock_device *clock) {
    return ((struct clock_device_data *)clock->clock_data)->preset_msecs;
}


static clock_device rtc_clock = { 
    .clock_data = &clock_device_data,
    .get_seconds_since_epoch = get_rtc_seconds_since_epoch,
    .get_msecs_since_boot = get_rtc_msecs_since_boot
};

static clock_device fixed_clock = {
    .clock_data = &clock_device_data,
    .get_seconds_since_epoch = get_fixed_seconds_since_epoch,
    .get_msecs_since_boot = get_fixed_msecs_since_boot
};

clock_device *new_rtc_based_clock_device() {
    return &rtc_clock;
}

clock_device *new_fixed_clock_device(uint32_t preset_seconds, uint32_t preset_msecs) {
    ((struct clock_device_data *)fixed_clock.clock_data)->preset_seconds = preset_seconds;
    ((struct clock_device_data *)fixed_clock.clock_data)->preset_msecs = preset_msecs;
    return &fixed_clock;
}
