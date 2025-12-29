#include "font8x16.h"


size font8x16_get_text_size(font8x16 *font, const char *text) {
    if (text == 0 || text[0] == 0) return size_zero();

    int width = 0;
    while (*text) {
        const glyph8x16 *gl = font8x16_get_glyph(font, *text++);
        width += gl->width + font->char_spacing;
    }
    if (width > 0) // remove last spacing
        width -= font->char_spacing;
    return size_of(width, font->line_height);
}
