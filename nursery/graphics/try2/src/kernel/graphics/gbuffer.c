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

// assume 32M color pallette, 3 bytes per pixel
#define GBUFFER_24BIT_PIXEL_AT(gb, x, y)                (gb->buffer + ((y) * gb->pitch) + ((x) * 3))
#define GBUFFER_SET_24BIT_PIXEL(ptr, r, g, b)           { *ptr++ = b; *ptr++ = g; *ptr++ = r; }
#define GBUFFER_SET_24BIT_PIXELS(ptr, r, g, b, count)   while (count-- > 0) { *ptr++ = b; *ptr++ = g; *ptr++ = r; }
#define GBUFFER_GET_24BIT_PIXEL(ptr)                    (((color)ptr[0]) | ((color)ptr[1] << 8) | ((color)ptr[2] << 16))
#define GBUFFER_SKIP_24BIT_PIXEL(ptr)                   { ptr += 3; }
#define GBUFFER_COPY_24BIT_PIXEL(dest, src)             { *dest++ = *src++; *dest++ = *src++; *dest++ = *src++; }
#define GBUFFER_COPY_24BIT_PIXELS(dest, src, count)     while (count-- > 0) { *dest++ = *src++; *dest++ = *src++; *dest++ = *src++; }
#define GBUFFER_BREAKUP_COLOR(clr)                      uint8_t red = RGB_R(clr); uint8_t green = RGB_G(clr); uint8_t blue  = RGB_B(clr);


void gb_set_pixel(gbuffer *gb, int x, int y, color clr) {
    if (x < 0 || x >= gb->width)  return;
    if (y < 0 || y >= gb->height) return;

    GBUFFER_BREAKUP_COLOR(clr);
    uint8_t *pixel_ptr = GBUFFER_24BIT_PIXEL_AT(gb, x, y);
    GBUFFER_SET_24BIT_PIXEL(pixel_ptr, red, green, blue);
}

color gb_get_pixel(gbuffer *gb, int x, int y) {
    if (x < 0 || x >= gb->width)  return 0;
    if (y < 0 || y >= gb->height) return 0;

    uint8_t *pixel_ptr = GBUFFER_24BIT_PIXEL_AT(gb, x, y);
    return GBUFFER_GET_24BIT_PIXEL(pixel_ptr);
}

void gb_fill(gbuffer *gb, color clr) {
    if (clr == 0) {
        memset(gb->buffer, clr & 0xFF, gb->buffer_size);
        return;
    }

    GBUFFER_BREAKUP_COLOR(clr);
    for (int i = 0; i < gb->height; i++) {
        uint8_t *pixel_ptr = GBUFFER_24BIT_PIXEL_AT(gb, 0, i);
        int count = gb->width;
        GBUFFER_SET_24BIT_PIXELS(pixel_ptr, red, green, blue, count);
    }
}

void gb_fill_rect(gbuffer *gb, int x, int y, int width, int height, color clr) {
    if (x >= gb->width)  return;
    if (y >= gb->height) return;
    if (width <= 0)      return;
    if (height <= 0)     return;

    if (x < 0) {
        width += x; // diminish width by the negative amount
        x = 0;
    }
    if (y < 0) {
        height += y; // diminish height by the negative amount
        y = 0;
    }
    if (x + width > gb->width)
        width = gb->width - x;
    if (y + height > gb->height)
        height = gb->height - y;

    GBUFFER_BREAKUP_COLOR(clr);

    int y_end = y + height;
    for (int i = y; i < y_end; i++) {
        uint8_t *pixel = GBUFFER_24BIT_PIXEL_AT(gb, x, i);
        int count = width;
        GBUFFER_SET_24BIT_PIXELS(pixel, red, green, blue, count);
    }
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

        uint8_t *src_pix  = GBUFFER_24BIT_PIXEL_AT(src, src_origin.x,  src_y);
        uint8_t *dest_pix = GBUFFER_24BIT_PIXEL_AT(dest, dest_origin.x, dest_y);
        int count = size.width;
        GBUFFER_COPY_24BIT_PIXELS(dest_pix, src_pix, count);
    }
}

static int gb_draw_8x16_character(gbuffer *gb, int x, int baseline_y, char chr, font8x16 *font, uint8_t red, uint8_t blue, uint8_t green) {
    const glyph8x16 *gl = font8x16_get_glyph(font, chr);

    for (int row_no = 0; row_no < font->line_height; row_no++) {
        uint8_t bitmap = gl->bitmaps[row_no];
        if (bitmap == 0)
            continue;

        uint8_t *pixel = GBUFFER_24BIT_PIXEL_AT(gb, x, baseline_y - font->baseline + row_no);
        uint8_t mask = 0x80;
        for (int column = 0; column < gl->width; column++) {
            if (bitmap & mask) {
                GBUFFER_SET_24BIT_PIXEL(pixel, red, green, blue);
            } else {
                GBUFFER_SKIP_24BIT_PIXEL(pixel);
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




// ---- above here all tested ---------

void gb_rect_border(gbuffer *gb, garea area, color clr) {
    GBUFFER_BREAKUP_COLOR(clr);
    uint8_t *pixel;
    int count;
    
    // top horizontal line
    pixel = GBUFFER_24BIT_PIXEL_AT(gb, area.origin.x, area.origin.y);
    count = area.size.width;
    GBUFFER_SET_24BIT_PIXELS(pixel, red, green, blue, count);

    // bottom horizontal line
    pixel = GBUFFER_24BIT_PIXEL_AT(gb, area.origin.x, area.origin.y + area.size.height);
    count = area.size.width;
    GBUFFER_SET_24BIT_PIXELS(pixel, red, green, blue, count);

    int y_end = area.origin.y + area.size.height;
    int right_x = area.origin.x + area.size.width - 1;
    for (int y = area.origin.y; y < y_end; y++) {
        // left column
        pixel = GBUFFER_24BIT_PIXEL_AT(gb, area.origin.x, y);
        GBUFFER_SET_24BIT_PIXEL(pixel, red, green, blue);

        // right columns
        pixel = GBUFFER_24BIT_PIXEL_AT(gb, right_x, y);
        GBUFFER_SET_24BIT_PIXEL(pixel, red, green, blue);
    }
}
