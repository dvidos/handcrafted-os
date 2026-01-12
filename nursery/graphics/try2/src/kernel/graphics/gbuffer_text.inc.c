#include "gbuffer.h"

static int gb_draw_8x16_character(gbuffer *gb, int x, int baseline_y, char chr, area clip, font8x16 *font, uint32_t clr) {
    const glyph8x16 *gl = font8x16_get_glyph(font, chr);

    // gb_rect(gb, area_of(x, baseline_y, 15, 1), clip, fill_params_solid(color_tango_red()), 0);

    for (int row_no = 0; row_no < font->num_bitmaps; row_no++) {
        uint8_t bitmap = gl->bitmaps[row_no];
        if (bitmap == 0)
            continue;

        point p = point_of(x, baseline_y - font->ascend + row_no);
        uint32_t *pixel = _pixel_pt_ptr(gb, p);
        uint8_t mask = 0x80;
        for (int column = 0; column < gl->width; column++) {
            if ((bitmap & mask) && area_contains(clip, p))
                _replace_pixel(pixel, clr);
            
            pixel += 1; // actually 4 bytes
            mask >>= 1;
            p.x += 1;
        }
    }

    return gl->width;
}

void gb_text(gbuffer *gb, area rect, area clip, const char *text, text_params params) {
    clip = area_intersect(clip, gb->area);
    if (area_is_empty(clip))
        return;

    area aligned_area = font8x16_text_align(params.font, text, rect, params.align);
    area text_area_clipped = area_intersect(aligned_area, clip);
    if (area_is_empty(text_area_clipped))
        return;

    // gb_border(gb, text_area, clip, 0, 1, color_tango_green());
    
    int x = aligned_area.x;
    int baseline_y = font8x6_get_vertically_centered_baseline_y(params.font, aligned_area);
    while (*text) {
        area glyph_area = font8x16_get_glyph_area(params.font, *text, x, baseline_y);
        // gb_rect(gb, glyph_area, clip, fill_params_solid(color_tango_bright_blue()), 0);
        if (!area_is_empty(area_intersect(glyph_area, text_area_clipped))) {
            gb_draw_8x16_character(gb, x, baseline_y, *text, text_area_clipped, params.font, params.color);
        }
        x += glyph_area.width + params.font->char_spacing;
        text++;
    }
}

void gb_text_demo(gbuffer *gb, area rect, font8x16 *font, color clr) {
    text_params tp = text_params_of(font, ALIGN_TOP_LEFT, clr);
    gb_text(gb, rect, rect, font->name, tp);
    rect = area_move(rect, 0, font->line_advance);

    gb_text(gb, rect, rect, "ABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890 {[(<>)]} \\|/", tp);
    rect = area_move(rect, 0, font->line_advance);

    gb_text(gb, rect, rect, "abcdefghijklmnopqrstuvwxyz `~!@#$%^&*-_=+;':\",.?", tp);
    rect = area_move(rect, 0, font->line_advance);

    gb_text(gb, rect, rect, "The quick brown fox jumped over the lazy dog!", tp);
    rect = area_move(rect, 0, font->line_advance);
}
