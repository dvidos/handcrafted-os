#pragma once
#include <stdint.h>

// A glyph is up to 9 rows high and up to 7 pixels wide
typedef struct {
    uint8_t width;      // width in pixels
    uint8_t bitmap[9];  // each byte represents a row, LSB = leftmost pixel
} glyph5x7;

const glyph5x7 *get_5x7_glyph(char c);