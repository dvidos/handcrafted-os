#include <stdint.h>
#include <stddef.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "gbuffer.h"


/*
    Inspiration and goals here: https://www.versionmuseum.com/history-of/classic-mac-os
    Scaling, antialiasing, semi-transparency, shadows, gradients, rounded corners.

    Afterwards, blit can support: 
    - simple, 
    - masked, 
    - bit-operations (and, or, xor),
    - format-changing (e.g. RGB565 -> XRGB8888),
    - scaling (using diff src/dest size),
    - alpha blending (dst = src * a + dst * (1 - a)

    For debugging he proposes:
    - a horizontal or vertical part of the screen dedicated for debugging messages, like a console
    - gb_debug_rect() with red, for debugging dimensions
    - gb_debug_crosshair() with red, for debugging points
    - allocate extra bytes on the buffers, with magic values, to detect overruns/underruns
*/

static inline uint32_t *_pixel_ptr(gbuffer *gb, int x, int y) { return gb->buffer_argb + (y * gb->width) + x; }
static inline uint32_t *_set_pixel(uint32_t *ptr, color clr)   { *ptr++ = clr; return ptr; }
static inline uint32_t *_set_pixel_row(uint32_t *ptr, uint32_t clr, int length)   { while (length-- > 0) { *ptr++ = clr; } return ptr; }
static inline color _get_pixel(uint32_t *ptr) { return (color)ptr; }
static inline uint32_t *_skip_pixel(uint32_t *ptr) { return ptr + 1; }
static inline void _copy_pixel_row(uint32_t *dest, uint32_t *src, int length) {  while (length-- > 0) { *dest++ = *src++; } }



gbuffer *new_gbuffer(int width, int height) {
    gbuffer *gb = (gbuffer *)kmalloc(sizeof(gbuffer));
    gb->width = width;
    gb->height = height;

    gb->buffer_size = height * width * sizeof(uint32_t);
    gb->buffer_argb = (uint32_t *)kmalloc(gb->buffer_size);

    return gb;
}

void gb_free(gbuffer *gb) {
    if (gb != 0) {
        kfree(gb->buffer_argb);
        kfree(gb);
    }
}

void gb_set_pixel(gbuffer *gb, int x, int y, color clr) {
    if (x < 0 || x >= gb->width)  return;
    if (y < 0 || y >= gb->height) return;
    _set_pixel(_pixel_ptr(gb, x, y), clr);
}

color gb_get_pixel(gbuffer *gb, int x, int y) {
    if (x < 0 || x >= gb->width)  return 0;
    if (y < 0 || y >= gb->height) return 0;
    return _get_pixel(_pixel_ptr(gb, x, y));
}

void gb_fill(gbuffer *gb, color clr) {
    if (clr == 0) {
        memset(gb->buffer_argb, clr & 0xFF, gb->buffer_size);
        return;
    }

    for (int i = 0; i < gb->height; i++) {
        uint32_t *pixel_ptr = _pixel_ptr(gb, 0, i);
        _set_pixel_row(pixel_ptr, clr, gb->width);
    }
}

void gb_fill_rect(gbuffer *gb, int x, int y, int width, int height, color clr) {
    if (x >= gb->width)  return;
    if (y >= gb->height) return;
    if (x < 0) { int diff = -x; width -= diff; x += diff; }
    if (y < 0) { int diff = -y; height -= diff; y += diff; }
    if (x + width  > gb->width)  { width  = gb->width  - x; }
    if (y + height > gb->height) { height = gb->height - y; }
    if (width  <= 0) return;
    if (height <= 0) return;

    int y_end = y + height;
    for (int i = y; i < y_end; i++) {
        uint32_t *pixel = _pixel_ptr(gb, x, i);
        _set_pixel_row(pixel, clr, width);
    }
}

void gb_fill_rect_rounded(gbuffer *gb, int x, int y, int width, int height, int radius, color clr) {

    color solid_color = color_with_alpha(0xFF, clr);
    color transparent = color_with_alpha(0x00, clr);

    // three rects, top, bottom, center, leaving the four corners unpainted
    gb_fill_rect(gb, x + radius, y,                   width - 2 * radius, radius, solid_color);
    gb_fill_rect(gb, x + radius, y + height - radius, width - 2 * radius, radius, solid_color);
    gb_fill_rect(gb, x, y + radius, width, height - 2 * radius, solid_color);

    // the -1 are there to avoid floating point arithmetic
    int squared_in_boundary  = (radius - 1) * (radius - 1);
    int squared_out_boundary = (radius - 0) * (radius - 0);
    int center_x1 = x + radius - 1;
    int center_y1 = y + radius - 1;
    int center_x2 = x + width - radius;
    int center_y2 = y + height - radius;
    color corner_clr;

    for (int dy = 0; dy <= radius; dy++) {
        for (int dx = 0; dx <= radius; dx++) {
            int squared_distance = dx*dx + dy*dy;
            if      (squared_distance <= squared_in_boundary ) corner_clr = solid_color;
            else if (squared_distance >= squared_out_boundary) corner_clr = transparent;
            else {
                uint8_t alpha = (squared_out_boundary - squared_distance) * 255 / (squared_out_boundary - squared_in_boundary);
                corner_clr = color_with_alpha(alpha, solid_color);
            }

            // paint symmetrically all four corners at once
            gb_set_pixel(gb, center_x1 - dx, center_y1 - dy, corner_clr); // top left
            gb_set_pixel(gb, center_x2 + dx, center_y1 - dy, corner_clr); // top right
            gb_set_pixel(gb, center_x2 + dx, center_y2 + dy, corner_clr); // botom right
            gb_set_pixel(gb, center_x1 - dx, center_y2 + dy, corner_clr); // botom left
        }
    }
}


void gb_rect_border(gbuffer *gb, int x, int y, int width, int height, color clr) {
    if (x >= gb->width)  return;
    if (y >= gb->height) return;
    if (x < 0) { int diff = -x; width -= diff; x += diff; }
    if (y < 0) { int diff = -y; height -= diff; y += diff; }
    if (x + width  > gb->width)  { width  = gb->width  - x; }
    if (y + height > gb->height) { height = gb->height - y; }
    if (width  <= 0) return;
    if (height <= 0) return;

    uint32_t *pixel;
    int count;
    int last_y = y + height - 1;
    int last_x = x + width - 1;

    // top line
    pixel = _pixel_ptr(gb, x, y);
    _set_pixel_row(pixel, clr, width);

    // bottom line
    pixel = _pixel_ptr(gb, x, y + height - 1);
    _set_pixel_row(pixel, clr, width);

    // left line
    for (int i = y; i <= last_y; i++) {
        pixel = _pixel_ptr(gb, x, i);
        _set_pixel(pixel, clr);
    }

    // right line
    for (int i = y; i <= last_y; i++) {
        pixel = _pixel_ptr(gb, last_x, i);
        _set_pixel(pixel, clr);
    }
}

static int gb_draw_8x16_character(gbuffer *gb, int x, int baseline_y, char chr, font8x16 *font, uint32_t clr) {
    const glyph8x16 *gl = font8x16_get_glyph(font, chr);

    for (int row_no = 0; row_no < font->num_bitmaps; row_no++) {
        uint8_t bitmap = gl->bitmaps[row_no];
        if (bitmap == 0)
            continue;

        uint32_t *pixel = _pixel_ptr(gb, x, baseline_y - font->baseline + row_no);
        uint8_t mask = 0x80;
        for (int column = 0; column < gl->width; column++) {
            if (bitmap & mask) {
                pixel = _set_pixel(pixel, clr);
            } else {
                pixel = _skip_pixel(pixel);
            }
            mask >>= 1;
        }
    }

    return gl->width;
}

int gb_text(gbuffer *gb, const char *text, int x, int base_y, font8x16 *f, color clr) {
    int running_x = x;
    int width = 0;

    // to make this very performant, maintain 16 pointers and advance them to the right.
    while (*text) {
        width = gb_draw_8x16_character(gb, running_x, base_y, *text, f, clr);
        running_x += width + f->char_spacing;
        text++;
    }

    return running_x - x;
}

void gb_text_demo(gbuffer *gb, int x, int baseline_y, font8x16 *font, color clr) {
    gb_text(gb, font->name, x, baseline_y, font, clr);
    baseline_y += font->line_height;
    gb_text(gb, "ABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890 {[(<>)]} \\|/", x, baseline_y, font, clr);
    baseline_y += font->line_height;
    gb_text(gb, "abcdefghijklmnopqrstuvwxyz `~!@#$%^&*-_=+;':\",.?", x, baseline_y, font, clr);
    baseline_y += font->line_height;
    gb_text(gb, "The quick brown fox jumped over the lazy dog!", x, baseline_y, font, clr);
}

void gb_copy_area(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin) {

    // if origins outside of boundaries, no point
    if (src_origin.x  >= dest->width)  return;
    if (src_origin.y  >= dest->height) return;
    if (dest_origin.x >= dest->width)  return;
    if (dest_origin.y >= dest->height) return;

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->width)   size.width  = src->width   - src_origin.x;
    if (src_origin.y  + size.height > src->height)  size.height = src->height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->width)  size.width  = dest->width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->height) size.height = dest->height - dest_origin.y;

    // is there anything visible left to copy?
    if (size.width  <= 0) return;
    if (size.height <= 0) return;

    for (int y_offs = 0; y_offs < size.height; y_offs++) {
        int src_y = src_origin.y + y_offs;
        int dest_y = dest_origin.y + y_offs;

        uint32_t *src_pix  = _pixel_ptr(src, src_origin.x,  src_y);
        uint32_t *dest_pix = _pixel_ptr(dest, dest_origin.x, dest_y);
        _copy_pixel_row(dest_pix, src_pix, size.width);
    }
}

void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, gsize size, gpoint dest_origin, gpoint src_origin, uint8_t global_alpha) {

    // if origins outside of boundaries, no point
    if (src_origin.x  >= dest->width)  return;
    if (src_origin.y  >= dest->height) return;
    if (dest_origin.x >= dest->width)  return;
    if (dest_origin.y >= dest->height) return;

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->width)   size.width  = src->width   - src_origin.x;
    if (src_origin.y  + size.height > src->height)  size.height = src->height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->width)  size.width  = dest->width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->height) size.height = dest->height - dest_origin.y;

    // is there anything visible left to copy?
    if (size.width  <= 0) return;
    if (size.height <= 0) return;

    for (int y_offs = 0; y_offs < size.height; y_offs++) {
        int src_y = src_origin.y + y_offs;
        int dest_y = dest_origin.y + y_offs;

        for (int x_offs = 0; x_offs < size.width; x_offs++) {
            uint32_t *src_pix  = _pixel_ptr(src, src_origin.x + x_offs,  src_y);
            uint32_t *dest_pix = _pixel_ptr(dest, dest_origin.x + x_offs, dest_y);

            uint8_t src_alpha = color_a(*src_pix) * global_alpha / 255;
            color blended = color_argb(
                color_a(*dest_pix), // unchanged
                color_blend_channel(color_r(*dest_pix), color_r(*src_pix), src_alpha),
                color_blend_channel(color_g(*dest_pix), color_g(*src_pix), src_alpha),
                color_blend_channel(color_b(*dest_pix), color_b(*src_pix), src_alpha)
            );
            _set_pixel(dest_pix, blended);
        }
    }
}
