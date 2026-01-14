#include "gbuffer_internal.h"

typedef struct alpha_interface alpha_interface_t;
typedef struct {
    uint8_t (*resolve)(alpha_interface_t *iface, point p, paint_section_t section);
    union {
        struct {
            int radius;
            int squared_in_boundary;
            int squared_out_boundary;
            int squared_bounds_diff;
        } rounded_corner;
        struct {
            int radius;
            int thickness;
            int inner_radius_in_boundary_sq;
            int inner_radius_out_boundary_sq;
            int inner_radius_sq_diff;
            int outer_radius_in_boundary_sq;
            int outer_radius_out_boundary_sq;
            int outer_radius_sq_diff;
        } rounded_border;
    } data;
} alpha_interface_t;

// -----------------------------------------------------

static uint8_t _rounded_corner_alpha_resolver(alpha_interface_t *iface, point p, paint_section_t section) {
    
    point center;
    int far = iface->data.rounded_corner.radius - 1;
    switch (section) {
        case SECTION_TOP_LEFT:     center = point_of(far, far); break;
        case SECTION_TOP_RIGHT:    center = point_of(0,   far); break;
        case SECTION_BOTTOM_LEFT:  center = point_of(far, 0);   break;
        case SECTION_BOTTOM_RIGHT: center = point_of(0,   0);   break;
        default: return 0x00;
    }
    
    int squared_distance = vector_squared_distance(vector_from_to(center, p));
    uint8_t alpha;
    if      (squared_distance <= iface->data.rounded_corner.squared_in_boundary) alpha = 0xFF;
    else if (squared_distance >= iface->data.rounded_corner.squared_out_boundary) alpha = 0;
    else    alpha = (iface->data.rounded_corner.squared_out_boundary - squared_distance) * 0xFF / iface->data.rounded_corner.squared_bounds_diff;
    
    return alpha;
}

static uint8_t _rounded_border_alpha_resolver(alpha_interface_t *iface, point p, paint_section_t section) {
    
    point center;
    int far = iface->data.rounded_border.radius - 1;
    switch (section) {
        case SECTION_TOP_LEFT:     center = point_of(far, far); break;
        case SECTION_TOP_RIGHT:    center = point_of(0,   far); break;
        case SECTION_BOTTOM_LEFT:  center = point_of(far, 0);   break;
        case SECTION_BOTTOM_RIGHT: center = point_of(0,   0);   break;
        default: return 0x00;
    }
    
    int squared_distance = vector_squared_distance(vector_from_to(center, p));
    uint8_t alpha;
    if (squared_distance < iface->data.rounded_border.inner_radius_in_boundary_sq)
        alpha = 0; // inside border

    else if (squared_distance >= iface->data.rounded_border.inner_radius_in_boundary_sq && squared_distance < iface->data.rounded_border.inner_radius_out_boundary_sq) 
        alpha = (squared_distance - iface->data.rounded_border.inner_radius_in_boundary_sq) * 0xFF / iface->data.rounded_border.inner_radius_sq_diff;

    else if (squared_distance >= iface->data.rounded_border.inner_radius_in_boundary_sq && squared_distance < iface->data.rounded_border.outer_radius_in_boundary_sq) 
        alpha = 0xFF; // on border zone

    else if (squared_distance >= iface->data.rounded_border.outer_radius_in_boundary_sq && squared_distance < iface->data.rounded_border.outer_radius_out_boundary_sq) 
        alpha = (iface->data.rounded_border.outer_radius_out_boundary_sq - squared_distance) * 0xFF / iface->data.rounded_border.outer_radius_sq_diff;

    else if (squared_distance > iface->data.rounded_border.outer_radius_in_boundary_sq) 
        alpha = 0; // outsize border
    
    return alpha;
}

static uint8_t _square_corner_alpha_resolver(alpha_interface_t *iface, point p, paint_section_t section) {
    return 0xFF;
}

// -----------------------------------------------------

static alpha_interface_t alpha_interface_for_rounded_corners(int radius) {
    int sq_in = (radius - 1) * (radius - 1);
    int sq_out = (radius - 0) * (radius - 0);

    return (alpha_interface_t){
        .resolve = _rounded_corner_alpha_resolver,
        .data = {
            .rounded_corner = {
                .radius = radius,
                .squared_in_boundary = sq_in,
                .squared_out_boundary = sq_out,
                .squared_bounds_diff = sq_out - sq_in
            }
        }
    };
}

static alpha_interface_t alpha_interface_for_rounded_borders(int radius, int thickness) {

    int inner_radius = radius < thickness ? 0 : radius - thickness;
    int outer_radius = radius;

    int inner_in_sq = (inner_radius - 1) * (inner_radius - 1);
    int inner_out_sq = (inner_radius - 0) * (inner_radius - 0);
    int outer_in_sq = (outer_radius - 1) * (outer_radius - 1);
    int outer_out_sq = (outer_radius - 0) * (outer_radius - 0);

    return (alpha_interface_t){
        .resolve = _rounded_border_alpha_resolver,
        .data = {
            .rounded_border = {
                .radius = radius,
                .thickness = thickness,
                .inner_radius_in_boundary_sq = inner_in_sq,
                .inner_radius_out_boundary_sq = inner_out_sq,
                .inner_radius_sq_diff = inner_out_sq - inner_in_sq,
                .outer_radius_in_boundary_sq = outer_in_sq,
                .outer_radius_out_boundary_sq = outer_out_sq,
                .outer_radius_sq_diff = outer_out_sq - outer_in_sq
            }
        }
    };
}

static alpha_interface_t alpha_interface_for_squared_corners() {
    return (alpha_interface_t){
        .resolve = _square_corner_alpha_resolver
    };
}
