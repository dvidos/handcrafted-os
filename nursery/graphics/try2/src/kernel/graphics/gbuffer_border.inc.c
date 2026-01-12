#include "gbuffer.h"



void gb_border(gbuffer *gb, area rect, area clip, int radius, int border_width, color clr) {
    clip = area_intersect(clip, gb->area);
    if (area_is_empty(clip))
        return;

    area rect_clipped = area_intersect(rect, clip); 
    if (area_is_empty(rect_clipped))
        return;

    if (radius < 0 || border_width <= 0)
        return;

    fill_params clr_prm = fill_params_solid(clr);

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

void gb_border2(gbuffer *gb, area rect, area clip, border_params params) {
    // we really need to revamp this...
}


