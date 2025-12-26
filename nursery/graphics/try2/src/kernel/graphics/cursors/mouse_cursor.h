#pragma once
#include <stdint.h>


typedef struct {
    uint32_t and_mask[32];
    uint32_t xor_mask[32];
    int hot_x;
    int hot_y;
} cursor32;

extern const cursor32 arrow_cursor;
