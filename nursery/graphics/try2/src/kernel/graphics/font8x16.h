#pragma once
#include <stdint.h>



typedef struct glyph8x16 {
    uint8_t width;
    uint8_t bitmaps[16];
} glyph8x16;

typedef struct {
    const char *name;
    uint8_t num_bitmaps;  // includes both ascenders and descenders
    uint8_t baseline;     // offset from top to baseline of letters
    uint8_t line_height;  // how far ahead two baselines should be
    uint8_t char_spacing; // 1-2 pixels for space between chars
    glyph8x16 *glyphs; // from space (32) to ~ (127).
    // some special glyphs (e.g. W) will have extension glyph, to save space
} font8x16;


extern font8x16 *mits7;
extern font8x16 *geneva9;
extern font8x16 *geneva9_bold;
extern font8x16 *geneva9_mono;

const glyph8x16 *font8x16_get_glyph(font8x16 *font, char c);