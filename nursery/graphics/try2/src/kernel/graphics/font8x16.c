#include "font8x16.h"


const glyph8x16 *font8x16_get_glyph(font8x16 *font, char c) {
    if (c < 32 || c > 127)
        return &font->glyphs[0];
    return &font->glyphs[c - 32];
}

