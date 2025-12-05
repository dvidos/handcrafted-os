#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct clock_device clock_device;

struct clock_device {
    uint32_t (*get_seconds_since_epoch)(clock_device *clock);
    void *clock_data;
};

clock_device *new_rtc_based_clock_device();
clock_device *new_fixed_clock_device(uint32_t preset_seconds);
