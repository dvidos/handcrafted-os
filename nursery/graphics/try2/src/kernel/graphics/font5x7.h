#pragma once
#include <stdint.h>

// A glyph is up to 7 rows high and up to 5 pixels wide
typedef struct {
    uint8_t width;      // width in pixels
    uint8_t bitmap[9];  // each byte represents a row, LSB = leftmost pixel
} glyph5x7;

// extern const glyph5x7 font5x7[95];

// static inline const glyph5x7 *get_glyph(char c) {
//     if (c < 32 || c > 126) return &font5x7[0]; // return space for unknown
//     return &font5x7[c - 32];
// }

