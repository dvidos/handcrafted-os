#include "gbuffer.h"


typedef void fill_func(gbuffer *gb, area rect, fill_params cp);


static color _location_dependent_color(point p, fill_params cp) {
    if (cp.fill_type == FILL_TYPE_SOLID)
        return cp.clr;
    
    if (cp.fill_type == FILL_TYPE_LINEAR_GRADIENT) {
        // given a pixel, say p, find dot product to find projected distance along gradient_v
        // then, normalize the distance to the gradient color space
        vectorf pixel_v = vectorf_from_to(cp.gradient_p1, p);
        float proj_distance = vectorf_dot_product(pixel_v, cp.gradient_v);
        float factor = clamp01(proj_distance / cp.gradient_len_sq);
        return color_gradient(cp.clr, cp.clr2, cp.ease(factor));
    }

    return 0;
}

static inline void _fill_rect_fast(gbuffer *gb, area rect, fill_params cp) {
    int y_end = rect.y + rect.height;
    for (int i = rect.y; i < y_end; i++)
        _set_pixel_row(_pixel_ptr(gb, rect.x, i), cp.clr, rect.width);
}

static inline void _fill_rect_slow(gbuffer *gb, area rect, fill_params cp) {
    int y_end = rect.y + rect.height;
    int x_end = rect.x + rect.width;
    for (int y = rect.y; y < y_end; y++) {
        for (int x = rect.x; x < x_end; x++) {
            _replace_pixel(_pixel_ptr(gb, x, y), _location_dependent_color(point_of(x, y), cp));
        }
    }
}

void gb_rect(gbuffer *gb, area rect, area clip, fill_params clr_prm, int radius) {
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

