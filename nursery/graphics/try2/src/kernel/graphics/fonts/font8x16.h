#pragma once
#include "../../fundamentals.h"
#include "../geometry.h"



typedef struct glyph8x16 {
    uint8_t width;
    uint8_t bitmaps[16];
} glyph8x16;

typedef struct {
    const char *name;
    uint8_t num_bitmaps;   // includes both ascenders and descenders
    uint8_t char_spacing;  // 1-2 pixels for space between chars
    uint8_t ascend;        // from baseline to top of highest glyph (e.g. 10)
    uint8_t descend;       // from baseline to bottom of lowest glyph (e.g. 3)
    uint8_t line_advance;  // how far ahead two baselines of two rows of text should be  (e.g. 18)
    glyph8x16 *glyphs;     // from space (32) to ~ (127).
    // some special glyphs (e.g. W) will have extension glyph, to save space
} font8x16;

extern font8x16 *mits7;
extern font8x16 *geneva9;
extern font8x16 *geneva9_bold;
extern font8x16 *geneva9_mono;

static inline const glyph8x16 *font8x16_get_glyph(font8x16 *font, char c) {
    if (c < 32 || c > 127)
        return &font->glyphs[0];
    return &font->glyphs[c - 32];
}

static inline size font8x16_get_glyph_size(font8x16 *font, char c) {
    const glyph8x16 *gl = font8x16_get_glyph(font, c);
    return size_of(gl->width, font->line_advance);
}

static inline area font8x16_get_glyph_area(font8x16 *font, char c, int x, int baseline_y) {
    const glyph8x16 *gl = font8x16_get_glyph(font, c);
    return area_of(x, baseline_y - font->ascend, gl->width, font->ascend + font->descend);
}

size font8x16_get_text_size(font8x16 *font, const char *text);
size font8x16_get_visual_text_size(font8x16 *font, const char *text);
area font8x16_text_align(font8x16 *font, const char *text, area container, alignment text_alignment);
int font8x6_get_vertically_centered_baseline_y(font8x16 *font, area box);
