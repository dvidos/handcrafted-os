#pragma once
#include "../../fundamentals.h"


typedef struct {
    uint32_t bitmaps[32];
    int size; // e.g. 16x16, 24x24, 32x32 etc
} icon32;

extern const icon32 icon_x16;
extern const icon32 icon_window16;
extern const icon32 icon_down16;
