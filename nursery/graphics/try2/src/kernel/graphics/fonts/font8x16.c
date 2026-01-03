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
    return size_of(width, font->ascend + font->descend);
}

area font8x16_text_align(font8x16 *font, const char *text, area container, alignment text_alignment) {
    // the problem comes from vertically centering, where the descends are scarce, therefore the letters appear higher.

    size text_size = font8x16_get_text_size(font, text);
    area aligned = area_align(container, text_size, text_alignment);

    if (text_alignment == ALIGN_MIDDLE_LEFT || text_alignment == ALIGN_MIDDLE_CENTER || text_alignment == ALIGN_MIDDLE_RIGHT) {
        // for this special case, we'll center the ascends, without the descends. Ideally the "M" height (as ascends may be a bit higher)
        aligned.y = container.y + (container.height - font->ascend) / 2;
    }

    return aligned;
}

int font8x6_get_vertically_centered_baseline_y(font8x16 *font, area box) {
    return box.y + 
        (box.height - (font->ascend + font->descend)) / 2 +   // add half of whilespace left
        font->ascend; // from top of highest glyph to base line, by definition
}
