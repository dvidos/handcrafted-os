#include <stdint.h>
#include <stddef.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "gbuffer.h"

/*
    GPT proposes:
    - put_pixel(), get_pixel()
    - fill(), but per scanline, don't assume lines continuity, memset only for zero
    - fill_rect(), with again writing per scanline
    - blit(), or copy source/dest, origin/origin, size. memcpy() per scanline.
        (blit = BIT BIT BLOCK TRANSFER)
    - scroll_y(), essentially an internal blit(), faster than rerendering text, clean new area
    - for text, can prepare a large glyph buffer with all fonts, 1bpp or 8bpp and copy that instead of rendering... (hmm...)

    I should focus on correctness first, performance later.
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

// assume 32M color pallette, 3 bytes per pixel
#define GBUFFER_BREAKUP_COLOR(clr)                      uint8_t red = color_r(clr); uint8_t green = color_g(clr); uint8_t blue  = color_b(clr);

static inline uint8_t *_pixel_ptr(gbuffer *gb, int x, int y) { return gb->buffer + y * gb->pitch + x * 3; }
static inline uint8_t *_set_pixel(uint8_t *ptr, uint8_t r, uint8_t g, uint8_t b)   { *ptr++ = b; *ptr++ = g; *ptr++ = r; return ptr; }
static inline uint8_t *_set_pixels(uint8_t *ptr, uint8_t r, uint8_t g, uint8_t b, int count)   { while (count-- > 0) { *ptr++ = b; *ptr++ = g; *ptr++ = r; } return ptr; }
static inline color _get_pixel(uint8_t *ptr) { return color_argb(0xFF, ptr[2], ptr[1], ptr[0]); }
static inline uint8_t *_skip_pixel(uint8_t *ptr) { return ptr + 3; }
static inline void _copy_pixels(uint8_t *dest, uint8_t *src, int count) {  while (count-- > 0) { *dest++ = *src++; *dest++ = *src++; *dest++ = *src++; } }



gbuffer *new_gbuffer(int width, int height, int pitch, int bits_per_pixel) {
    if (pitch < width)
        pitch = width;
    
    gbuffer *gb = (gbuffer *)kmalloc(sizeof(gbuffer));
    gb->width = width;
    gb->height = height;
    gb->pitch = pitch;
    gb->bits_per_pixel = bits_per_pixel;

    // we should have a specific method to set pixels, depending 
    int bytes_per_pixel = (bits_per_pixel + 7) / 8; // ceiling division

    gb->buffer_size = (pitch * height) * bytes_per_pixel;
    gb->buffer = (uint8_t *)kmalloc(gb->buffer_size);

    return gb;
}

void gb_free(gbuffer *gb) {
    if (gb != 0) {
        kfree(gb->buffer);
        kfree(gb);
    }
}

void gb_set_pixel(gbuffer *gb, int x, int y, color clr) {
    if (x < 0 || x >= gb->width)  return;
    if (y < 0 || y >= gb->height) return;

    GBUFFER_BREAKUP_COLOR(clr);
    uint8_t *pixel_ptr = _pixel_ptr(gb, x, y);
    _set_pixel(pixel_ptr, red, green, blue);
}

color gb_get_pixel(gbuffer *gb, int x, int y) {
    if (x < 0 || x >= gb->width)  return 0;
    if (y < 0 || y >= gb->height) return 0;

    uint8_t *pixel_ptr = _pixel_ptr(gb, x, y);
    return _get_pixel(pixel_ptr);
}

void gb_fill(gbuffer *gb, color clr) {
    if (clr == 0) {
        memset(gb->buffer, clr & 0xFF, gb->buffer_size);
        return;
    }

    GBUFFER_BREAKUP_COLOR(clr);
    for (int i = 0; i < gb->height; i++) {
        uint8_t *pixel_ptr = _pixel_ptr(gb, 0, i);
        int count = gb->width;
        pixel_ptr = _set_pixels(pixel_ptr, red, green, blue, count);
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

    GBUFFER_BREAKUP_COLOR(clr);

    int y_end = y + height;
    for (int i = y; i < y_end; i++) {
        uint8_t *pixel = _pixel_ptr(gb, x, i);
        int count = width;
        pixel = _set_pixels(pixel, red, green, blue, count);
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

    GBUFFER_BREAKUP_COLOR(clr);
    uint8_t *pixel;
    int count;
    int last_y = y + height - 1;
    int last_x = x + width - 1;

    // top line
    pixel = _pixel_ptr(gb, x, y);
    count = width;
    pixel = _set_pixels(pixel, red, green, blue, count);

    // bottom line
    pixel = _pixel_ptr(gb, x, y + height - 1);
    count = width;
    pixel = _set_pixels(pixel, red, green, blue, count);

    // left line
    for (int i = y; i <= last_y; i++) {
        uint8_t *pixel = _pixel_ptr(gb, x, i);
        _set_pixel(pixel, red, green, blue);
    }

    // right line
    for (int i = y; i <= last_y; i++) {
        uint8_t *pixel = _pixel_ptr(gb, last_x, i);
        _set_pixel(pixel, red, green, blue);
    }
}

static int gb_draw_8x16_character(gbuffer *gb, int x, int baseline_y, char chr, font8x16 *font, uint8_t red, uint8_t blue, uint8_t green) {
    const glyph8x16 *gl = font8x16_get_glyph(font, chr);

    for (int row_no = 0; row_no < font->num_bitmaps; row_no++) {
        uint8_t bitmap = gl->bitmaps[row_no];
        if (bitmap == 0)
            continue;

        uint8_t *pixel = _pixel_ptr(gb, x, baseline_y - font->baseline + row_no);
        uint8_t mask = 0x80;
        for (int column = 0; column < gl->width; column++) {
            if (bitmap & mask) {
                pixel = _set_pixel(pixel, red, green, blue);
            } else {
                pixel = _skip_pixel(pixel);
            }
            mask >>= 1;
        }
    }

    return gl->width;
}

int gb_text(gbuffer *gb, const char *text, int x, int base_y, font8x16 *f, color clr) {
    GBUFFER_BREAKUP_COLOR(clr);
    int running_x = x;
    int width = 0;

    while (*text) {
        width = gb_draw_8x16_character(gb, running_x, base_y, *text, f, red, green, blue);
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

        uint8_t *src_pix  = _pixel_ptr(src, src_origin.x,  src_y);
        uint8_t *dest_pix = _pixel_ptr(dest, dest_origin.x, dest_y);
        int count = size.width;
        // we need to advance the pointers...
        _copy_pixels(dest_pix, src_pix, count);
        dest_pix = _skip_pixel(dest_pix);
        src_pix = _skip_pixel(src_pix);

    }
}
