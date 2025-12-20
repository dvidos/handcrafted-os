#include <stdint.h>
#include <stddef.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "gbuffer.h"


#define clamp01(val)       ((val) > 1.0f) ? 1.0f : (((val) < 0.0f) ? 0.0f : (val))

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

static inline uint32_t *_pixel_ptr(gbuffer *gb, int x, int y) { return gb->buffer_argb + (y * gb->area.size.width) + x; }
static inline uint32_t *_set_pixel(uint32_t *ptr, color clr)   { *ptr++ = clr; return ptr; }
static inline uint32_t *_set_pixel_row(uint32_t *ptr, uint32_t clr, int length)   { while (length-- > 0) { *ptr++ = clr; } return ptr; }
static inline color _get_pixel(uint32_t *ptr) { return (color)ptr; }
static inline uint32_t *_skip_pixel(uint32_t *ptr) { return ptr + 1; }
static inline void _copy_pixel_row(uint32_t *dest, uint32_t *src, int length) {  while (length-- > 0) { *dest++ = *src++; } }

// ----------------------------------------------------

static gbuffer *global_aux_buffer = 0;
void gb_set_aux_buffer(gbuffer *aux_buffer) {
    // this buffer to be used for blurring, single threadedly
    global_aux_buffer = aux_buffer;
}

// ----------------------------------------------------

typedef struct blur_window {
    uint32_t alpha;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    int size;
} blur_window;
static inline void blur_window_reset(blur_window *w) {
    w->alpha = 0;
    w->red   = 0;
    w->green = 0;
    w->blue  = 0;
    w->size  = 0;
}
static inline void blur_window_add(blur_window *w, uint32_t *pixel) {
    w->alpha += color_a(*pixel);
    w->red   += color_r(*pixel);
    w->green += color_g(*pixel);
    w->blue  += color_b(*pixel);
    w->size  += 1;
}
static inline void blur_window_remove(blur_window *w, uint32_t *pixel) {
    w->alpha -= color_a(*pixel);
    w->red   -= color_r(*pixel);
    w->green -= color_g(*pixel);
    w->blue  -= color_b(*pixel);
    w->size  -= 1;
}
static inline void blur_window_collect_horizontally(blur_window *w, int curr_x, int y, int radius, gbuffer *gb) {
    for (int offset = -radius; offset <= +radius; offset++) {
        if (curr_x + offset < 0) continue;
        if (curr_x + offset >= gb->area.size.width) continue;
        uint32_t *pixel = _pixel_ptr(gb, curr_x + offset, y);
        blur_window_add(w, pixel);
    }
}
static inline void blur_window_collect_vertically(blur_window *w, int x, int curr_y, int radius, gbuffer *gb) {
    for (int offset = -radius; offset <= +radius; offset++) {
        if (curr_y + offset < 0) continue;
        if (curr_y + offset >= gb->area.size.height) continue;
        uint32_t *pixel = _pixel_ptr(gb, x, curr_y + offset);
        blur_window_add(w, pixel);
    }
}
static inline void blur_window_apply(blur_window *w, uint32_t *pixel, int do_blur_alpha) {
    if (w->size == 0)
        return;
    
    if (do_blur_alpha) {
        _set_pixel(pixel, color_argb(
            w->alpha / w->size,
            color_r(*pixel),
            color_g(*pixel),
            color_b(*pixel)
        ));
    } else {
        _set_pixel(pixel, color_argb(
            color_a(*pixel),
            w->red   / w->size,
            w->green / w->size,
            w->blue  / w->size
        ));
    }
}

// -------------------------------------------

gbuffer *new_gbuffer(int width, int height) {
    gbuffer *gb = (gbuffer *)kmalloc(sizeof(gbuffer));
    gb->area = garea_of(0, 0, width, height);
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

void gb_set_pixel(gbuffer *gb, gpoint p, color clr) {
    if (!gpoint_is_inside(p, gb->area)) return;
    _set_pixel(_pixel_ptr(gb, p.x, p.y), clr);
}

color gb_get_pixel(gbuffer *gb, gpoint p) {
    if (!gpoint_is_inside(p, gb->area)) return 0;
    return _get_pixel(_pixel_ptr(gb, p.x, p.y));
}

void gb_fill(gbuffer *gb, color clr) {
    if (clr == 0) {
        memset(gb->buffer_argb, clr & 0xFF, gb->buffer_size);
        return;
    }

    for (int i = 0; i < gb->area.size.height; i++) {
        uint32_t *pixel_ptr = _pixel_ptr(gb, 0, i);
        _set_pixel_row(pixel_ptr, clr, gb->area.size.width);
    }
}

void gb_fill_rect(gbuffer *gb, garea rect, color clr) {
    // if (x >= gb->area.size.width)  return;
    // if (y >= gb->area.size.height) return;
    // if (x < 0) { int diff = -x; width -= diff; x += diff; }
    // if (y < 0) { int diff = -y; height -= diff; y += diff; }
    // if (x + width  > gb->area.size.width)  { width  = gb->area.size.width  - x; }
    // if (y + height > gb->area.size.height) { height = gb->area.size.height - y; }
    // if (width  <= 0) return;
    // if (height <= 0) return;
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;

    int y_end = rect.origin.y + rect.size.height;
    for (int i = rect.origin.y; i < y_end; i++) {
        uint32_t *pixel = _pixel_ptr(gb, rect.origin.x, i);
        _set_pixel_row(pixel, clr, rect.size.width);
    }
}

void gb_fill_rect_rounded(gbuffer *gb, garea rect, int radius, color clr) {

    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;
    
    color solid_color = color_with_alpha(0xFF, clr);
    color transparent = color_with_alpha(0x00, clr);

    // three rects, top, bottom, center, leaving the four corners unpainted
    gb_fill_rect(gb, garea_of(rect.origin.x + radius, rect.origin.y,                             rect.size.width - 2 * radius, radius), solid_color);
    gb_fill_rect(gb, garea_of(rect.origin.x + radius, rect.origin.y + rect.size.height - radius, rect.size.width - 2 * radius, radius), solid_color);
    gb_fill_rect(gb, garea_of(rect.origin.x,          rect.origin.y + radius,                    rect.size.width,              rect.size.height - 2 * radius), solid_color);

    // the -1 are there to avoid floating point arithmetic
    int squared_in_boundary  = (radius - 1) * (radius - 1);
    int squared_out_boundary = (radius - 0) * (radius - 0);
    int center_x1 = rect.origin.x + radius - 1;
    int center_y1 = rect.origin.y + radius - 1;
    int center_x2 = rect.origin.x + rect.size.width - radius;
    int center_y2 = rect.origin.y + rect.size.height - radius;
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
            _set_pixel(_pixel_ptr(gb, center_x1 - dx, center_y1 - dy), corner_clr); // top left
            _set_pixel(_pixel_ptr(gb, center_x2 + dx, center_y1 - dy), corner_clr); // top right
            _set_pixel(_pixel_ptr(gb, center_x2 + dx, center_y2 + dy), corner_clr); // botom right
            _set_pixel(_pixel_ptr(gb, center_x1 - dx, center_y2 + dy), corner_clr); // botom left
        }
    }
}

void gb_rect_border(gbuffer *gb, garea rect, color clr) {

    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;

    uint32_t *pixel;
    int count;
    gpoint bot_right = garea_bottom_right(rect);

    // top line
    pixel = _pixel_ptr(gb, rect.origin.x, rect.origin.y);
    _set_pixel_row(pixel, clr, rect.size.width);

    // bottom line
    pixel = _pixel_ptr(gb, rect.origin.x, rect.origin.y + rect.size.height - 1);
    _set_pixel_row(pixel, clr, rect.size.width);

    // left line
    for (int i = rect.origin.y; i <= bot_right.y; i++) {
        pixel = _pixel_ptr(gb, rect.origin.x, i);
        _set_pixel(pixel, clr);
    }

    // right line
    for (int i = rect.origin.y; i <= bot_right.y; i++) {
        pixel = _pixel_ptr(gb, bot_right.x, i);
        _set_pixel(pixel, clr);
    }
}

void gb_blur(gbuffer *gb, garea rect, int radius, int do_blur_alpha) {
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;

    blur_window win;
    uint32_t *pixel;

    // copy some zone from the original image, into the aux buffer, 
    // so that vertical passes do not bleed into unknown pixels.
    // TODO: improve the blurring code, just up/down and left/right makes for poor results, need diagonal possibly weighted
    gb_copy_area(global_aux_buffer, gb, gsize_of(rect.size.width, radius), gpoint_of(rect.origin.x, rect.origin.y - radius), gpoint_of(rect.origin.x, rect.origin.y - radius));
    gb_copy_area(global_aux_buffer, gb, gsize_of(rect.size.width, radius), gpoint_of(rect.origin.x, rect.origin.y + rect.size.height), gpoint_of(rect.origin.x, rect.origin.y + rect.size.height));

    gpoint end = garea_bottom_right_exclusive(rect);

    // ok, the stupid, slow way first, blur horizontally first
    for (int curr_y = rect.origin.y; curr_y < end.y; curr_y++) {
        for (int curr_x = rect.origin.x; curr_x < end.x; curr_x++) {
            blur_window_reset(&win);
            // read from buffer, write to aux_buffer
            blur_window_collect_horizontally(&win, curr_x, curr_y, radius, gb);
            pixel = _pixel_ptr(global_aux_buffer, curr_x, curr_y);
            blur_window_apply(&win, pixel, do_blur_alpha);
        }
    }

    // then vertically
    for (int curr_x = rect.origin.x; curr_x < end.x; curr_x++) {
        for (int curr_y = rect.origin.y; curr_y < end.y; curr_y++) {
            blur_window_reset(&win);
            // read from aux_buffer, write to buffer
            blur_window_collect_vertically(&win, curr_x, curr_y, radius, global_aux_buffer);
            pixel = _pixel_ptr(gb, curr_x, curr_y);
            blur_window_apply(&win, pixel, do_blur_alpha);
        }
    }
}

void gb_gradient_rect(gbuffer *gb, garea rect, gpoint g1, gpoint g2, color c1, color c2, ease_function ease) {
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;

    // gradients are passed as related to rect, but we paint as related to buffer. convert them.
    g1 = gpoint_to_global(g1, rect);
    g2 = gpoint_to_global(g2, rect);
    gvector gradient_v = gvector_from_to(g1, g2);
    float gradient_len_sq = gvector_dot(gradient_v, gradient_v);

    // scan line by line
    gpoint endpoint = garea_bottom_right_exclusive(rect);
    for (int y = rect.origin.y; y < endpoint.y; y++) {
        uint32_t *ptr = _pixel_ptr(gb, rect.origin.x, y);

        for (int x = rect.origin.x; x < endpoint.x; x++) {

            // given a pixel, say p, find dot product to find projected distance along gradient_v
            gpoint p = gpoint_of(x, y);
            gvector pixel_v = gvector_from_to(g1, p);
            float proj_distance = gvector_dot(pixel_v, gradient_v);
            
            float normalized = clamp01(proj_distance / gradient_len_sq);
            normalized = ease(normalized);

            color gradient = color_gradient(c1, c2, normalized);
            _set_pixel(ptr++, gradient);
        }
    }
}

void gb_rect_border_rounded(gbuffer *gb, garea rect, int radius, int border_width, color clr) {
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;
    if (radius <= 0 || border_width <= 0)
        return;

    // Draw straight rectangle edges first
    color solid = color_with_alpha(0xFF, clr);
    gb_fill_rect(gb, garea_of(rect.origin.x + radius, rect.origin.y, rect.size.width - 2 * radius, border_width), solid);
    gb_fill_rect(gb, garea_of(rect.origin.x + radius, rect.origin.y + rect.size.height - border_width, rect.size.width - 2 * radius, border_width), solid);
    gb_fill_rect(gb, garea_of(rect.origin.x, rect.origin.y + radius, border_width, rect.size.height - 2 * radius), solid);
    gb_fill_rect(gb, garea_of(rect.origin.x + rect.size.width - border_width, rect.origin.y + radius, border_width, rect.size.height - 2 * radius), solid);

    int center_x1 = rect.origin.x + radius - 1;
    int center_y1 = rect.origin.y + radius - 1;
    int center_x2 = rect.origin.x + rect.size.width - radius;
    int center_y2 = rect.origin.y + rect.size.height - radius;

    float inner_radius = radius - border_width;
    float inner_radius_in_boundary_sq  = (inner_radius - 1) * (inner_radius - 1);
    float inner_radius_out_boundary_sq = (inner_radius - 0) * (inner_radius - 0);
    float inner_radius_sq_diff = inner_radius_out_boundary_sq - inner_radius_in_boundary_sq;
    float outer_radius = radius;
    float outer_radius_in_boundary_sq  = (outer_radius - 1) * (outer_radius - 1);
    float outer_radius_out_boundary_sq = (outer_radius - 0) * (outer_radius - 0);
    float outer_radius_sq_diff = outer_radius_out_boundary_sq - outer_radius_in_boundary_sq;

    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {

            float dxf = dx;
            float dyf = dy;
            float distance_sq = dxf * dxf + dyf * dyf;

            float alpha = 0.0f;
            if (distance_sq < inner_radius_in_boundary_sq)
                alpha = 0.0f; // inside border
            else if (distance_sq >= inner_radius_in_boundary_sq && distance_sq < inner_radius_out_boundary_sq) 
                alpha = (distance_sq - inner_radius_in_boundary_sq) / inner_radius_sq_diff;
            else if (distance_sq >= inner_radius_in_boundary_sq && distance_sq < outer_radius_in_boundary_sq) 
                alpha = 1.0f; // on border zone
            else if (distance_sq >= outer_radius_in_boundary_sq && distance_sq < outer_radius_out_boundary_sq) 
                alpha = (outer_radius_out_boundary_sq - distance_sq) / outer_radius_sq_diff;
            else if (distance_sq > outer_radius_in_boundary_sq) 
                alpha = 0.0f; // outsize border
            alpha = clamp01(alpha);

            // paint symmetrically all four corners at once
            color px_color = color_with_alpha_factor(alpha, clr);
            _set_pixel(_pixel_ptr(gb, center_x1 - dx, center_y1 - dy), px_color); // top left
            _set_pixel(_pixel_ptr(gb, center_x2 + dx, center_y1 - dy), px_color); // top right
            _set_pixel(_pixel_ptr(gb, center_x2 + dx, center_y2 + dy), px_color); // botom right
            _set_pixel(_pixel_ptr(gb, center_x1 - dx, center_y2 + dy), px_color); // botom left
        }
    }
}

static void gb_make_drop_shadow(gbuffer *gb, const gbuffer *object, int offset, int blur_radius) {

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
    if (src_origin.x  >= dest->area.size.width)  return;
    if (src_origin.y  >= dest->area.size.height) return;
    if (dest_origin.x >= dest->area.size.width)  return;
    if (dest_origin.y >= dest->area.size.height) return;

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->area.size.width)   size.width  = src->area.size.width   - src_origin.x;
    if (src_origin.y  + size.height > src->area.size.height)  size.height = src->area.size.height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->area.size.width)  size.width  = dest->area.size.width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->area.size.height) size.height = dest->area.size.height - dest_origin.y;

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
    if (src_origin.x  >= dest->area.size.width)  return;
    if (src_origin.y  >= dest->area.size.height) return;
    if (dest_origin.x >= dest->area.size.width)  return;
    if (dest_origin.y >= dest->area.size.height) return;

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->area.size.width)   size.width  = src->area.size.width   - src_origin.x;
    if (src_origin.y  + size.height > src->area.size.height)  size.height = src->area.size.height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->area.size.width)  size.width  = dest->area.size.width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->area.size.height) size.height = dest->area.size.height - dest_origin.y;

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
