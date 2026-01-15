#include "gbuffer_internal.h"


void initialize_gbuffer(gbuffer *aux_buffer) {
    // this buffer to be used for blurring, single threadedly
    global_aux_buffer = aux_buffer;
    // we should also prepare gausian distribution tables, floats that sum up to 1.
}


// TODO: after main functions, make a dialog demonstrating the various aspects, having keys flip the various features:
// TODO: after fixing the graphics print, do a surface that can do a popup window.
#include "v3.inc.c"

#include "blur.inc.c"
#include "rect.inc.c"
#include "border.inc.c"
#include "text.inc.c"



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


void gb_icon(gbuffer *gb, area rect, area clip, const icon32 *icon, color clr) {
    clip = area_intersect(clip, gb->area);
    if (area_is_empty(clip))
        return;

    area rect_clipped = area_intersect(rect, clip); 
    if (area_is_empty(rect_clipped))
        return;

    for (int row = 0; row < icon->size; row++) {
        uint32_t bitmap = icon->bitmaps[row];
        if (bitmap == 0) continue;

        point p = point_of(rect.x, rect.y + row);
        uint32_t *pixel = _pixel_pt_ptr(gb, p);
        uint32_t mask = 0x80000000;
        for (int col = 0; col < 32; col++) {
            if ((bitmap & mask) && area_contains(clip, p))
                _replace_pixel(pixel, clr);
    
            pixel += 1; // actually 4 bytes
            mask >>= 1;
            p.x += 1;
        }
    }
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
