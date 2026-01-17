#include "gbuffer_internal.h"


#include "alpha.inc.c"
#include "chroma.inc.c"
#include "painter.inc.c"


static void _debug_area(gbuffer *gb, area a) {
    int dash_size = 4;
    int count = 0;
    color c1 = 0x7F000000;
    color c2 = 0x7FFFFFFF;
    bool mark = true;
    color clr = c1;

    int far_y = a.y + a.height - 1;
    for (int x = a.x; x < a.x + a.width; x++) {
        if (++count > dash_size) { mark = !mark; clr = mark ? c1 : c2; count = 0; }
        _blend_pixel(_pixel_ptr(gb, x, a.y), clr);
        _blend_pixel(_pixel_ptr(gb, x, far_y), clr);
    }

    int far_x = a.x + a.width - 1;
    for (int y = a.y; y < a.y + a.height; y++) {
        if (++count > dash_size) { mark = !mark; clr = mark ? c1 : c2; count = 0; }
        _blend_pixel(_pixel_ptr(gb, a.x, y), clr);
        _blend_pixel(_pixel_ptr(gb, far_x, y), clr);
    }
}

static inline void paint_area_v3(painter_interface_t *painter, area section_area, paint_section_t section, area clip) {
    area draw = area_intersect(section_area, clip);
    if (area_is_empty(draw)) return;

    painter->prepare(painter, section_area, section);
    for (int y = draw.y; y < draw.y + draw.height; y++) {
        painter->paint(painter, point_of(draw.x, y), draw.width);
    }
}

void gb_draw_border_v3(gbuffer *gb, area rect, area clip, border_params params, bool show_sections, bool show_alpha) {
    if (params.style == BORDER_NONE || area_is_empty(rect) || area_is_empty(clip) || params.thickness <= 0 || params.radius < 0)
        return;
    
    int corner_size = params.radius > 0 ? params.radius : params.thickness;
    area top, bottom, left, right, top_left, top_right, bottom_left, bottom_right;
    area_split_rounded_border_areas(rect, corner_size, params.thickness, &top, &bottom, &left, &right, &top_left, &top_right, &bottom_left, &bottom_right);

    chroma_interface_t chroma;
    alpha_interface_t alpha;
    painter_interface_t painter;

    // choose optimal path, if simple border
    chroma = (params.style == BORDER_FLAT) ?
        chroma_interface_for_solid_color(params.clr) :
        chroma_interface_for_3d_border(params.style, params.clr, params.contrast_3d, params.thickness, rect.width, rect.height);
    alpha = alpha_interface_for_opaque_areas();
    painter = show_alpha ? painter_interface_for_alpha_heat_map(gb, &alpha) : 
        ((params.style == BORDER_FLAT) ?
            painter_interface_for_same_color_in_row(gb, &chroma, &alpha) :
            painter_interface_for_different_color_per_pixel(gb, &chroma, &alpha));
    paint_area_v3(&painter, top,    SECTION_TOP,    clip);
    paint_area_v3(&painter, left,   SECTION_LEFT,   clip);
    paint_area_v3(&painter, right,  SECTION_RIGHT,  clip);
    paint_area_v3(&painter, bottom, SECTION_BOTTOM, clip);

    // corners are tricky, if 3D or rounded
    alpha = (params.radius == 0) ?
        alpha_interface_for_opaque_areas() :
        alpha_interface_for_rounded_borders(params.radius, params.thickness);
    painter = show_alpha ? 
        painter_interface_for_alpha_heat_map(gb, &alpha) :
        painter_interface_for_different_color_per_pixel(gb, &chroma, &alpha);

    paint_area_v3(&painter, top_left,     SECTION_TOP_LEFT,     clip);
    paint_area_v3(&painter, top_right,    SECTION_TOP_RIGHT,    clip);
    paint_area_v3(&painter, bottom_left,  SECTION_BOTTOM_LEFT,  clip);
    paint_area_v3(&painter, bottom_right, SECTION_BOTTOM_RIGHT, clip);

    if (show_sections) {
        _debug_area(gb, top);
        _debug_area(gb, left);
        _debug_area(gb, right);
        _debug_area(gb, bottom);
        _debug_area(gb, top_left);
        _debug_area(gb, top_right);
        _debug_area(gb, bottom_left);
        _debug_area(gb, bottom_right);
    }
}

void gb_fill_rect_v3(gbuffer *gb, area rect, area clip, int radius, fill_params params, bool show_sections, bool show_alpha) {
    if (params.fill_type == FILL_TYPE_NONE || area_is_empty(rect) || area_is_empty(clip) || radius < 0)
        return;

    area top, middle, bottom, top_left, top_right, bottom_left, bottom_right;
    area_split_rounded_fill_areas(rect, radius, &top, &middle, &bottom, &top_left, &top_right, &bottom_left, &bottom_right);

    chroma_interface_t chroma;
    alpha_interface_t alpha;
    painter_interface_t painter;

    // rectangular areas can be optimized
    chroma = (params.fill_type == FILL_TYPE_SOLID) ?
        chroma_interface_for_solid_color(params.clr) :
        chroma_interface_for_gradient(params);
    alpha = alpha_interface_for_opaque_areas();
    painter = show_alpha ? painter_interface_for_alpha_heat_map(gb, &alpha) : 
        ((params.fill_type == FILL_TYPE_SOLID) ?
            painter_interface_for_same_color_in_row(gb, &chroma, &alpha) :
            painter_interface_for_different_color_per_pixel(gb, &chroma, &alpha));

    paint_area_v3(&painter, top,    SECTION_TOP,    clip);
    paint_area_v3(&painter, middle, SECTION_CENTER, clip);
    paint_area_v3(&painter, bottom, SECTION_BOTTOM, clip);

    if (radius > 0) {
        alpha = alpha_interface_for_rounded_corners(radius);
        if (show_alpha)
            painter = painter_interface_for_alpha_heat_map(gb, &alpha);
        else
            painter = painter_interface_for_different_color_per_pixel(gb, &chroma, &alpha);

        paint_area_v3(&painter, top_left,     SECTION_TOP_LEFT,     clip);
        paint_area_v3(&painter, top_right,    SECTION_TOP_RIGHT,    clip);
        paint_area_v3(&painter, bottom_left,  SECTION_BOTTOM_LEFT,  clip);
        paint_area_v3(&painter, bottom_right, SECTION_BOTTOM_RIGHT, clip);
    }

    if (show_sections) {
        _debug_area(gb, top);
        _debug_area(gb, middle);
        _debug_area(gb, bottom);
        _debug_area(gb, top_left);
        _debug_area(gb, top_right);
        _debug_area(gb, bottom_left);
        _debug_area(gb, bottom_right);
    }
}


