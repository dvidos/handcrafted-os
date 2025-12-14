#pragma once
#include <stdint.h>

// A glyph is up to 9 rows high and up to 7 pixels wide
typedef struct {
    uint8_t width;        // width in pixels
    uint16_t bitmap[16];  // each byte represents a row, LSB = leftmost pixel
} glyph16x16;

const glyph16x16 *get_16x16_glyph(char c);