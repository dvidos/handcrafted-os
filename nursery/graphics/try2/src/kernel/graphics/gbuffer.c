#include <stdint.h>
#include <stddef.h>
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "../devs/serial.h"
#include "gbuffer.h"


#define clamp01(val)       ((val) > 1.0f) ? 1.0f : (((val) < 0.0f) ? 0.0f : (val))
#define clamp255(val)      ((val) > 255) ? 255 : (((val) < 0) ? 0 : (val))

static inline uint32_t *_pixel_ptr(const gbuffer *gb, int x, int y) { return gb->buffer_argb + (y * gb->area.size.width) + x; }
static inline uint32_t *_replace_pixel(uint32_t *ptr, color clr)   { *ptr++ = clr; return ptr; }
static inline uint32_t *_blend_pixel(uint32_t *ptr, color clr)   { *ptr++ = color_blend(*ptr, clr); return ptr; }
static inline uint32_t *_set_pixel_row(uint32_t *ptr, uint32_t clr, int length)   { while (length-- > 0) { *ptr++ = clr; } return ptr; }
static inline color _get_pixel(uint32_t *ptr) { return (color)*ptr; }
static inline uint32_t *_skip_pixel(uint32_t *ptr) { return ptr + 1; }
static inline void _copy_pixel_row(uint32_t *dest, uint32_t *src, int length) {  while (length-- > 0) { *dest++ = *src++; } }

// gaussian blur functions
#include "gbuffer_blur.inc.c"


static color _location_dependent_color(gpoint p, color_params cp) {
    if (cp.fill_type == FILL_TYPE_SOLID)
        return cp.clr;
    
    if (cp.fill_type == FILL_TYPE_LINEAR_GRADIENT) {
        // given a pixel, say p, find dot product to find projected distance along gradient_v
        // then, normalize the distance to the gradient color space
        gvector pixel_v = gvector_from_to(cp.gradient_p1, p);
        float proj_distance = gvector_dot(pixel_v, cp.gradient_v);
        float factor = clamp01(proj_distance / cp.gradient_len_sq);
        return color_gradient(cp.clr, cp.clr2, cp.ease(factor));
    }

    return 0;
}

typedef void fill_func(gbuffer *gb, garea rect, color_params cp);

static inline void _fill_rect_fast(gbuffer *gb, garea rect, color_params cp) {
    int y_end = rect.origin.y + rect.size.height;
    for (int i = rect.origin.y; i < y_end; i++)
        _set_pixel_row(_pixel_ptr(gb, rect.origin.x, i), cp.clr, rect.size.width);
}

static inline void _fill_rect_slow(gbuffer *gb, garea rect, color_params cp) {
    int y_end = rect.origin.y + rect.size.height;
    int x_end = rect.origin.x + rect.size.width;
    for (int y = rect.origin.y; y < y_end; y++) {
        for (int x = rect.origin.x; x < x_end; x++) {
            _replace_pixel(_pixel_ptr(gb, x, y), _location_dependent_color(gpoint_of(x, y), cp));
        }
    }
}




// ----------------------------------------------------

static gbuffer *global_aux_buffer = 0;
void initialize_gbuffer(gbuffer *aux_buffer) {
    // this buffer to be used for blurring, single threadedly
    global_aux_buffer = aux_buffer;
    // we should also prepare gausian distribution tables, floats that sum up to 1.
}

// ----------------------------------------------------

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

void gb_clear(gbuffer *gb) {
    // zero makes everything transparent
    memset(gb->buffer_argb, 0, gb->buffer_size);
}

color gb_get_pixel(gbuffer *gb, gpoint p) {
    if (!gpoint_is_inside(p, gb->area)) return 0;
    return _get_pixel(_pixel_ptr(gb, p.x, p.y));
}

void gb_paint_pixel(gbuffer *gb, gpoint p, color clr) {
    if (!gpoint_is_inside(p, gb->area)) return;
    _blend_pixel(_pixel_ptr(gb, p.x, p.y), clr);
}

void gb_fill(gbuffer *gb, color clr) {
    for (int y = 0; y < gb->area.size.height; y++) {
        _set_pixel_row(_pixel_ptr(gb, 0, y), clr, gb->area.size.width);
    }
}

void gb_rect(gbuffer *gb, garea rect, color_params cp, int radius) {

    cp.gradient_p1 = gpoint_to_global(cp.gradient_p1, rect);
    cp.gradient_p2 = gpoint_to_global(cp.gradient_p2, rect);

    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;
    if (radius < 0)
        return;
    
    fill_func *rect_filler = (cp.fill_type == FILL_TYPE_SOLID) ? _fill_rect_fast : _fill_rect_slow;
    if (radius == 0) {
        rect_filler(gb, rect, cp);
    } else if (radius > 0) {
        // three rects, top, bottom, center, leaving the four corners unpainted
        rect_filler(gb, garea_of(rect.origin.x + radius, rect.origin.y,                             rect.size.width - 2 * radius, radius                       ), cp);
        rect_filler(gb, garea_of(rect.origin.x + radius, rect.origin.y + rect.size.height - radius, rect.size.width - 2 * radius, radius                       ), cp);
        rect_filler(gb, garea_of(rect.origin.x,          rect.origin.y + radius,                    rect.size.width,              rect.size.height - 2 * radius), cp);

        int squared_in_boundary  = (radius - 1) * (radius - 1);
        int squared_out_boundary = (radius - 0) * (radius - 0);
        int cx1 = rect.origin.x + radius - 1;
        int cy1 = rect.origin.y + radius - 1;
        int cx2 = rect.origin.x + rect.size.width - radius;
        int cy2 = rect.origin.y + rect.size.height - radius;

        // we'll need to derive both color and alpha, based on x,y pixel
        for (int dy = 0; dy <= radius; dy++) {
            for (int dx = 0; dx <= radius; dx++) {
                
                // same alpha is shared on all four corners
                uint8_t alpha;
                int squared_distance = dx*dx + dy*dy;
                if      (squared_distance <= squared_in_boundary) alpha = 0xFF;
                else if (squared_distance >= squared_out_boundary) alpha = 0;
                else {
                    alpha = (squared_out_boundary - squared_distance) * 255 / (squared_out_boundary - squared_in_boundary);
                }

                // but the four points may have different color due to gradient
                gpoint pt_tl = gpoint_of(cx1 - dx, cy1 - dy);
                gpoint pt_tr = gpoint_of(cx2 + dx, cy1 - dy);
                gpoint pt_br = gpoint_of(cx2 + dx, cy2 + dy);
                gpoint pt_bl = gpoint_of(cx1 - dx, cy2 + dy);

                color clr_tl = color_with_alpha(alpha, _location_dependent_color(pt_tl, cp));
                color clr_tr = color_with_alpha(alpha, _location_dependent_color(pt_tr, cp));
                color clr_br = color_with_alpha(alpha, _location_dependent_color(pt_br, cp));
                color clr_bl = color_with_alpha(alpha, _location_dependent_color(pt_bl, cp));

                _blend_pixel(_pixel_ptr(gb, cx1 - dx, cy1 - dy), clr_tl); // top left
                _blend_pixel(_pixel_ptr(gb, cx2 + dx, cy1 - dy), clr_tr); // top right
                _blend_pixel(_pixel_ptr(gb, cx2 + dx, cy2 + dy), clr_br); // botom right
                _blend_pixel(_pixel_ptr(gb, cx1 - dx, cy2 + dy), clr_bl); // botom left
            }
        }
    }
}

void gb_blur(gbuffer *gb, garea rect, int radius, int blur_alpha_instead_of_color) {
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;
    if (radius <= 0)
        return;
    
    /*
        update: i used gaussian blur, which is ideal mathematically, but painfully slow.
        i even had to create external tool to generate precal tables for radii 1..32.
        it seems box blur (x3) achieves similar results. box blur is essentially adding average of sourounding pixels.
        one pass horizontal, one vertical, then again, two more times (or more).
        so, box blur is a building block, not a final product.
        if one uses a running window, speed can be improved dramatically.
        the window only works in the "middle", where the size of the window can be full.
        i should make a building block (actually two, for x/y directions) in the include file,
        then call it 6 times here.
    */

    // copy some zone from the original image, into the aux buffer, 
    // so that vertical passes do not bleed into unknown pixels.
    gb_copy_area(global_aux_buffer, gb, gsize_of(rect.size.width, radius), gpoint_of(rect.origin.x, rect.origin.y - radius),           gpoint_of(rect.origin.x, rect.origin.y - radius));
    gb_copy_area(global_aux_buffer, gb, gsize_of(rect.size.width, radius), gpoint_of(rect.origin.x, rect.origin.y + rect.size.height), gpoint_of(rect.origin.x, rect.origin.y + rect.size.height));

    blur_window_apply_func *color_applicator = (blur_alpha_instead_of_color ? blur_window_apply_alpha : blur_window_apply_color);

    for (int times = 0; times < 3; times++) {
        blur_window_box_algorithm(
            gb, global_aux_buffer, 
            rect.origin.y, rect.origin.y + rect.size.height,
            rect.origin.x, rect.origin.x + rect.size.width,
            radius, blur_get_pixel_horizontal_slices, color_applicator
        );
        blur_window_box_algorithm(
            global_aux_buffer, gb,
            rect.origin.x, rect.origin.x + rect.size.width,
            rect.origin.y, rect.origin.y + rect.size.height,
            radius, blur_get_pixel_vertical_slices, color_applicator
        );
    }
}

void gb_border(gbuffer *gb, garea rect, int radius, int border_width, color clr) {
    rect = garea_crop(rect, gb->area);
    if (garea_is_empty(rect))
        return;
    if (radius < 0 || border_width <= 0)
        return;

    color_params cp = color_params_solid(clr);

    // Draw straight rectangle edges first
    _fill_rect_fast(gb, garea_of(rect.origin.x + radius, rect.origin.y, rect.size.width - 2 * radius, border_width), cp);
    _fill_rect_fast(gb, garea_of(rect.origin.x + radius, rect.origin.y + rect.size.height - border_width, rect.size.width - 2 * radius, border_width), cp);
    _fill_rect_fast(gb, garea_of(rect.origin.x, rect.origin.y + radius, border_width, rect.size.height - 2 * radius), cp);
    _fill_rect_fast(gb, garea_of(rect.origin.x + rect.size.width - border_width, rect.origin.y + radius, border_width, rect.size.height - 2 * radius), cp);

    if (radius == 0)
        return;
    
    int center_x1 = rect.origin.x + radius - 1;
    int center_y1 = rect.origin.y + radius - 1;
    int center_x2 = rect.origin.x + rect.size.width - radius;
    int center_y2 = rect.origin.y + rect.size.height - radius;

    // there's 5 zones: totally transparent, antialiased, solid, antialiased, transparent
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
            color pixel_color = color_with_alpha_factor(alpha, clr);
            _blend_pixel(_pixel_ptr(gb, center_x1 - dx, center_y1 - dy), pixel_color); // top left
            _blend_pixel(_pixel_ptr(gb, center_x2 + dx, center_y1 - dy), pixel_color); // top right
            _blend_pixel(_pixel_ptr(gb, center_x2 + dx, center_y2 + dy), pixel_color); // botom right
            _blend_pixel(_pixel_ptr(gb, center_x1 - dx, center_y2 + dy), pixel_color); // botom left
        }
    }
}

void gb_drop_shadow(gbuffer *gb, const gbuffer *object, shadow_params params) {
    // make sure we have space to draw
    if (gb->area.size.width  < object->area.size.width  + params.offset_x + 2 * params.blur_radius ||
        gb->area.size.height < object->area.size.height + params.offset_y + 2 * params.blur_radius)
        return;

    // offset and merge alpha
    for (int y = 0; y < object->area.size.height; y++) {

        for (int x = 0; x < object->area.size.width; x++) {
            uint32_t *object_pix = _pixel_ptr(object, x, y);
            uint8_t object_alpha = color_a(*object_pix);
            if (object_alpha == 0)
                continue;
            
            uint8_t shadow_alpha = (object_alpha * params.opacity) / 255;
            uint32_t *target_pix = _pixel_ptr(gb, x + params.offset_x, y + params.offset_y);
            _replace_pixel(target_pix, color_with_alpha(shadow_alpha, params.clr));
        }
    }

    // finally, blur alpha
    gb_blur(gb, gb->area, params.blur_radius, 1);
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
                pixel = _replace_pixel(pixel, clr);
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
            _replace_pixel(dest_pix, blended);
        }
    }
}
