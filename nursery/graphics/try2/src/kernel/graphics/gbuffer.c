#include <stdint.h>
#include <stddef.h>
#include "../concepts/logger.h"
#include "../memory/malloc.h"
#include "../memory/string.h"
#include "../devs/serial.h"
#include "gbuffer.h"
#include "cursors/mouse_cursor.h"


#define clamp01(val)       ((val) > 1.0f) ? 1.0f : (((val) < 0.0f) ? 0.0f : (val))
#define clamp255(val)      ((val) > 255) ? 255 : (((val) < 0) ? 0 : (val))

static inline uint32_t *_pixel_ptr(const gbuffer *gb, int x, int y) { return gb->buffer_argb + (y * gb->area.width) + x; }
static inline uint32_t *_pixel_pt_ptr(const gbuffer *gb, point p) { return gb->buffer_argb + (p.y * gb->area.width) + p.x; }
static inline uint32_t *_replace_pixel(uint32_t *ptr, color clr)   { *ptr++ = clr; return ptr; }
static inline uint32_t *_blend_pixel(uint32_t *ptr, color clr)   { *ptr++ = color_blend(*ptr, clr); return ptr; }
static inline uint32_t *_set_pixel_row(uint32_t *ptr, uint32_t clr, int length)   { while (length-- > 0) { *ptr++ = clr; } return ptr; }
static inline color _get_pixel(uint32_t *ptr) { return (color)*ptr; }
static inline uint32_t *_skip_pixel(uint32_t *ptr) { return ptr + 1; }
static inline void _copy_pixel_row(uint32_t *dest, uint32_t *src, int length) {  while (length-- > 0) { *dest++ = *src++; } }

// gaussian blur functions
#include "gbuffer_blur.inc.c"


static color _location_dependent_color(point p, color_params cp) {
    if (cp.fill_type == FILL_TYPE_SOLID)
        return cp.clr;
    
    if (cp.fill_type == FILL_TYPE_LINEAR_GRADIENT) {
        // given a pixel, say p, find dot product to find projected distance along gradient_v
        // then, normalize the distance to the gradient color space
        vector pixel_v = vector_from_to(cp.gradient_p1, p);
        float proj_distance = vector_dot_product(pixel_v, cp.gradient_v);
        float factor = clamp01(proj_distance / cp.gradient_len_sq);
        return color_gradient(cp.clr, cp.clr2, cp.ease(factor));
    }

    return 0;
}

typedef void fill_func(gbuffer *gb, area rect, color_params cp);

static inline void _fill_rect_fast(gbuffer *gb, area rect, color_params cp) {
    int y_end = rect.y + rect.height;
    for (int i = rect.y; i < y_end; i++)
        _set_pixel_row(_pixel_ptr(gb, rect.x, i), cp.clr, rect.width);
}

static inline void _fill_rect_slow(gbuffer *gb, area rect, color_params cp) {
    int y_end = rect.y + rect.height;
    int x_end = rect.x + rect.width;
    for (int y = rect.y; y < y_end; y++) {
        for (int x = rect.x; x < x_end; x++) {
            _replace_pixel(_pixel_ptr(gb, x, y), _location_dependent_color(point_of(x, y), cp));
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
    gb->area = area_of(0, 0, width, height);
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

color gb_get_pixel(gbuffer *gb, point p) {
    if (!point_is_inside(p, gb->area)) return 0;
    return _get_pixel(_pixel_ptr(gb, p.x, p.y));
}

void gb_paint_pixel(gbuffer *gb, point p, color clr) {
    if (!point_is_inside(p, gb->area)) return;
    _blend_pixel(_pixel_ptr(gb, p.x, p.y), clr);
}

void gb_fill(gbuffer *gb, color clr) {
    for (int y = 0; y < gb->area.height; y++) {
        _set_pixel_row(_pixel_ptr(gb, 0, y), clr, gb->area.width);
    }
}

void gb_rect(gbuffer *gb, area rect, area clip, color_params clr_prm, int radius) {
    clip = area_intersect(clip, gb->area);
    if (area_is_empty(clip))
        return;

    area rect_clipped = area_intersect(rect, clip); 
    if (area_is_empty(rect_clipped))
        return;

    clr_prm.gradient_p1 = point_to_global(clr_prm.gradient_p1, rect);
    clr_prm.gradient_p2 = point_to_global(clr_prm.gradient_p2, rect);
    if (radius < 0)
        return;
    
    fill_func *rect_filler = (clr_prm.fill_type == FILL_TYPE_SOLID) ? _fill_rect_fast : _fill_rect_slow;
    if (radius == 0) {
        rect_filler(gb, rect_clipped, clr_prm);
    } else if (radius > 0) {
        // three rects, top, bottom, center, leaving the four corners unpainted
        area top = area_of(rect.x + radius, rect.y,                        rect.width - 2 * radius, radius                  );
        area mid = area_of(rect.x,          rect.y + radius,               rect.width,              rect.height - 2 * radius);
        area bot = area_of(rect.x + radius, rect.y + rect.height - radius, rect.width - 2 * radius, radius                  );
        area top_clipped = area_intersect(top, clip);
        area mid_clipped = area_intersect(mid, clip);
        area bot_clipped = area_intersect(bot, clip);
        if (!area_is_empty(top_clipped)) rect_filler(gb, top_clipped, clr_prm);
        if (!area_is_empty(mid_clipped)) rect_filler(gb, mid_clipped, clr_prm);
        if (!area_is_empty(bot_clipped)) rect_filler(gb, bot_clipped, clr_prm);

        int squared_in_boundary  = (radius - 1) * (radius - 1);
        int squared_out_boundary = (radius - 0) * (radius - 0);
        int cx1 = rect.x + radius - 1;
        int cy1 = rect.y + radius - 1;
        int cx2 = rect.x + rect.width - radius;
        int cy2 = rect.y + rect.height - radius;
        point pt;
        color clr;

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
                
                pt = point_of(cx1 - dx, cy1 - dy);  // top left
                if (area_contains(rect_clipped, pt)) {
                    clr = color_with_alpha(alpha, _location_dependent_color(pt, clr_prm));
                    _blend_pixel(_pixel_pt_ptr(gb, pt), clr);
                }
                
                pt = point_of(cx2 + dx, cy1 - dy); // top right
                if (area_contains(rect_clipped, pt)) {
                    clr = color_with_alpha(alpha, _location_dependent_color(pt, clr_prm));
                    _blend_pixel(_pixel_pt_ptr(gb, pt), clr);
                }
                
                pt = point_of(cx2 + dx, cy2 + dy); // bottom right
                if (area_contains(rect_clipped, pt)) {
                    clr = color_with_alpha(alpha, _location_dependent_color(pt, clr_prm));
                    _blend_pixel(_pixel_pt_ptr(gb, pt), clr);
                }
                
                pt = point_of(cx1 - dx, cy2 + dy);  // bottom left
                if (area_contains(rect_clipped, pt)) {
                    clr = color_with_alpha(alpha, _location_dependent_color(pt, clr_prm));
                    _blend_pixel(_pixel_pt_ptr(gb, pt), clr);
                }
            }
        }
    }
}

void gb_border(gbuffer *gb, area rect, area clip, int radius, int border_width, color clr) {
    clip = area_intersect(clip, gb->area);
    if (area_is_empty(clip))
        return;

    area rect_clipped = area_intersect(rect, clip); 
    if (area_is_empty(rect_clipped))
        return;

    if (radius < 0 || border_width <= 0)
        return;

    color_params clr_prm = color_params_solid(clr);

    // Draw straight rectangle edges first
    area top_border = area_of(rect.x + radius, rect.y, rect.width - 2 * radius, border_width);
    area lft_border = area_of(rect.x, rect.y + radius, border_width, rect.height - 2 * radius);
    area rgt_border = area_of(rect.x + rect.width - border_width, rect.y + radius, border_width, rect.height - 2 * radius);
    area bot_border = area_of(rect.x + radius, rect.y + rect.height - border_width, rect.width - 2 * radius, border_width);
    area top_border_clipped = area_intersect(top_border, clip);
    area lft_border_clipped = area_intersect(lft_border, clip);
    area rgt_border_clipped = area_intersect(rgt_border, clip);
    area bot_border_clipped = area_intersect(bot_border, clip);
    if (!area_is_empty(top_border_clipped)) _fill_rect_fast(gb, top_border_clipped, clr_prm);
    if (!area_is_empty(lft_border_clipped)) _fill_rect_fast(gb, lft_border_clipped, clr_prm);
    if (!area_is_empty(rgt_border_clipped)) _fill_rect_fast(gb, rgt_border_clipped, clr_prm);
    if (!area_is_empty(bot_border_clipped)) _fill_rect_fast(gb, bot_border_clipped, clr_prm);

    if (radius <= 0)
        return; // no need for further processing
    
    int center_x1 = rect.x + radius - 1;
    int center_y1 = rect.y + radius - 1;
    int center_x2 = rect.x + rect.width - radius;
    int center_y2 = rect.y + rect.height - radius;

    // there's 5 zones: totally transparent, antialiased, solid, antialiased, transparent
    float inner_radius = radius < border_width ? 0 : radius - border_width;
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
            point pt;

            pt = point_of(center_x1 - dx, center_y1 - dy); // top left
            if (area_contains(rect_clipped, pt)) _blend_pixel(_pixel_pt_ptr(gb, pt), pixel_color); 
            
            pt = point_of(center_x2 + dx, center_y1 - dy); // top right
            if (area_contains(rect_clipped, pt)) _blend_pixel(_pixel_pt_ptr(gb, pt), pixel_color); 
            
            pt = point_of(center_x2 + dx, center_y2 + dy); // bottom right
            if (area_contains(rect_clipped, pt)) _blend_pixel(_pixel_pt_ptr(gb, pt), pixel_color); 
            
            pt = point_of(center_x1 - dx, center_y2 + dy); // bottom left
            if (area_contains(rect_clipped, pt)) _blend_pixel(_pixel_pt_ptr(gb, pt), pixel_color); 
        }
    }
}

void gb_blur(gbuffer *gb, area rect, int radius, int blur_alpha_instead_of_color) {
    rect = area_crop(rect, gb->area);
    if (area_is_empty(rect))
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
    gb_copy_area_fast(global_aux_buffer, gb, size_of(rect.width, radius), point_of(rect.x, rect.y - radius),           point_of(rect.x, rect.y - radius));
    gb_copy_area_fast(global_aux_buffer, gb, size_of(rect.width, radius), point_of(rect.x, rect.y + rect.height), point_of(rect.x, rect.y + rect.height));

    blur_window_apply_func *color_applicator = (blur_alpha_instead_of_color ? blur_window_apply_alpha : blur_window_apply_color);

    for (int times = 0; times < 3; times++) {
        blur_window_box_algorithm(
            gb, global_aux_buffer, 
            rect.y, rect.y + rect.height,
            rect.x, rect.x + rect.width,
            radius, blur_get_pixel_horizontal_slices, color_applicator
        );
        blur_window_box_algorithm(
            global_aux_buffer, gb,
            rect.x, rect.x + rect.width,
            rect.y, rect.y + rect.height,
            radius, blur_get_pixel_vertical_slices, color_applicator
        );
    }
}

void gb_drop_shadow(gbuffer *gb, const gbuffer *object, shadow_params params) {
    // make sure we have space to draw
    if (gb->area.width  < object->area.width  + params.offset_x + 2 * params.blur_radius ||
        gb->area.height < object->area.height + params.offset_y + 2 * params.blur_radius)
        return;

    // offset and merge alpha
    for (int y = 0; y < object->area.height; y++) {

        for (int x = 0; x < object->area.width; x++) {
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


void gb_draw_cursor32_fast(gbuffer *gb, point mouse_pos, const cursor32 *cursor) {
    // offset by hotspot
    mouse_pos.x -= cursor->hot_x;
    mouse_pos.y -= cursor->hot_y;

    for (int y = 0; y < 32; y++) {
        int pixel_y = mouse_pos.y + y;
        if (pixel_y < 0 || pixel_y >= gb->area.height)
            continue;
        
        uint32_t *pixel = _pixel_ptr(gb, mouse_pos.x, pixel_y);
        uint32_t and_row = cursor->and_mask[y];
        uint32_t xor_row = cursor->xor_mask[y];
        uint32_t bit = 0x80000000u;

        for (int x = 0; x < 32; x++, bit >>= 1, pixel++) { // note step actions
            int pixel_x = mouse_pos.x + x;
            if (pixel_x < 0 || pixel_x >= gb->area.width)
                continue;

            // AND=1, XOR=0 -> transparent
            // AND=0, XOR=0 -> black
            // AND=0, XOR=1 -> white
            // AND=1, XOR=1 -> invert (unused)

            uint32_t pixel_and_mask = (and_row & bit) ? 0xFFFFFFFFu : 0xFF000000u;
            uint32_t pixel_xor_mask = (xor_row & bit) ? 0x00FFFFFFu : 0x00000000u;
            *pixel = ((*pixel & pixel_and_mask) ^ pixel_xor_mask);
        }
    }
}

void gb_copy_area_fast(gbuffer *dest, gbuffer *src, size size, point dest_origin, point src_origin) {
    // if origins outside of boundaries, no point
    if (src_origin.x  >= src->area.width)   { return; }
    if (src_origin.y  >= src->area.height)  { return; }
    if (dest_origin.x >= dest->area.width)  { return; }
    if (dest_origin.y >= dest->area.height) { return; }

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->area.width)   size.width  = src->area.width   - src_origin.x;
    if (src_origin.y  + size.height > src->area.height)  size.height = src->area.height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->area.width)  size.width  = dest->area.width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->area.height) size.height = dest->area.height - dest_origin.y;

    // is there anything visible left to copy?
    if (size.width  <= 0) { return; }
    if (size.height <= 0) { return; }

    for (int y_offs = 0; y_offs < size.height; y_offs++) {
        int src_y = src_origin.y + y_offs;
        int dest_y = dest_origin.y + y_offs;

        uint32_t *src_pix  = _pixel_ptr(src, src_origin.x,  src_y);
        uint32_t *dest_pix = _pixel_ptr(dest, dest_origin.x, dest_y);
        _copy_pixel_row(dest_pix, src_pix, size.width);
    }
}

void gb_copy_area_with_alpha(gbuffer *dest, gbuffer *src, size size, point dest_origin, point src_origin, uint8_t global_alpha) {

    // if origins outside of boundaries, no point
    if (src_origin.x  >= dest->area.width)  return;
    if (src_origin.y  >= dest->area.height) return;
    if (dest_origin.x >= dest->area.width)  return;
    if (dest_origin.y >= dest->area.height) return;

    // actually diminish sizes, if in negative values
    if (src_origin.x  < 0) { int d = -src_origin.x;  size.width  -= d; src_origin.x  += d; dest_origin.x += d; }
    if (src_origin.y  < 0) { int d = -src_origin.y;  size.height -= d; src_origin.y  += d; dest_origin.y += d; }
    if (dest_origin.x < 0) { int d = -dest_origin.x; size.width  -= d; dest_origin.x += d; src_origin.x  += d; }
    if (dest_origin.y < 0) { int d = -dest_origin.y; size.height -= d; dest_origin.y += d; src_origin.y  += d; }

    // shorten size, if bleeding outside
    if (src_origin.x  + size.width  > src->area.width)   size.width  = src->area.width   - src_origin.x;
    if (src_origin.y  + size.height > src->area.height)  size.height = src->area.height  - src_origin.y;
    if (dest_origin.x + size.width  > dest->area.width)  size.width  = dest->area.width  - dest_origin.x;
    if (dest_origin.y + size.height > dest->area.height) size.height = dest->area.height - dest_origin.y;

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

void gb_copy_area_to_framebuffer_with_bpp(gbuffer *gb, area area, void *dest_buffer, int dest_pitch, int dest_bpp) {

    if (dest_bpp == 32) {
        // we can use fast memcpy()
        for (int y = area.y; y < area.y + area.height; y++) {
            uint8_t *dest_ptr = (uint8_t *)dest_buffer + (y * dest_pitch) + area.x * 4;  // note: 4 bytes per pixel
            uint32_t *src_ptr = gb->buffer_argb + (y * gb->area.width) + area.x;    // note: buffer_argb is a (uint32*) not a (char*)
            int bytes = area.width * 4;

            memcpy(dest_ptr, src_ptr, bytes);
        }

    } else if (dest_bpp == 24) {
        // we'll have to convert
        for (int y = area.y; y < area.y + area.height; y++) {
            uint8_t *dest_ptr = (uint8_t *)dest_buffer + (y * dest_pitch) + area.x * 3;  // note: 3 bytes per pixel
            uint32_t *src_ptr = gb->buffer_argb + (y * gb->area.width) + area.x;    // note: buffer_argb is a (uint32*) not a (char*)
            int pixels = area.width;

            while (pixels-- > 0) {
                *dest_ptr++ = color_b(*src_ptr);
                *dest_ptr++ = color_g(*src_ptr);
                *dest_ptr++ = color_r(*src_ptr);
                src_ptr++; // note: pointer to uint32
            }
        }
    }
}