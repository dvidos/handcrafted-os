#pragma once
#include <stdint.h>

static inline uint32_t rand_r(uint32_t *seed) {
    *seed = (*seed * 1664525u) + 1013904223u;
    return *seed;
}

