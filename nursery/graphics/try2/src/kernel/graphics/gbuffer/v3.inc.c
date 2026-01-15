#include "gbuffer_internal.h"


#include "alpha.inc.c"
#include "chroma.inc.c"
#include "painter.inc.c"


static inline void paint_area_v3(painter_interface_t *painter, gbuffer *gb, area part, paint_section_t section, area clip) {
    area draw = area_intersect(part, clip);
    if (area_is_empty(draw)) return;

    for (int y = draw.y; y < draw.y + draw.height; y++)
        painter->paint(painter, gb, point_of(draw.x, y), draw.width, section);
}

void gb_draw_border_v3(gbuffer *gb, area rect, area clip, int radius, int thickness, border_style_t style, color base_color, factor contrast) {
    if (style == BORDER_NONE || area_is_empty(rect) || area_is_empty(clip) || thickness <= 0 || radius < 0)
        return;
    
    int corner_size = radius > 0 ? radius : thickness;
    area top, bottom, left, right, top_left, top_right, bottom_left, bottom_right;
    area_split_rounded_border_areas(rect, corner_size, thickness, &top, &bottom, &left, &right, &top_left, &top_right, &bottom_left, &bottom_right);

    chroma_interface_t chroma;
    alpha_interface_t alpha;
    painter_interface_t painter;

    // choose optimal path, if simple border
    chroma = (style == BORDER_FLAT) ?
        chroma_interface_for_solid_color(base_color) :
        chroma_interface_for_3d_border(style, base_color, contrast, thickness, rect.width, rect.height);
    alpha = alpha_interface_for_opaque_areas();
    painter = (style == BORDER_FLAT) ?
        painter_interface_for_same_color_in_row(&chroma, &alpha) :
        painter_interface_for_different_color_per_pixel(&chroma, &alpha);

    paint_area_v3(&painter, gb, top,    SECTION_TOP,    clip);
    paint_area_v3(&painter, gb, left,   SECTION_LEFT,   clip);
    paint_area_v3(&painter, gb, right,  SECTION_RIGHT,  clip);
    paint_area_v3(&painter, gb, bottom, SECTION_BOTTOM, clip);

    // corners are tricky, if 3D or rounded
    alpha = (radius == 0) ?
        alpha_interface_for_opaque_areas() :
        alpha_interface_for_rounded_borders(radius, thickness);
    painter = painter_interface_for_different_color_per_pixel(&chroma, &alpha);

    paint_area_v3(&painter, gb, top_left,     SECTION_TOP_LEFT,     clip);
    paint_area_v3(&painter, gb, top_right,    SECTION_TOP_RIGHT,    clip);
    paint_area_v3(&painter, gb, bottom_left,  SECTION_BOTTOM_LEFT,  clip);
    paint_area_v3(&painter, gb, bottom_right, SECTION_BOTTOM_RIGHT, clip);
}

void gb_fill_rect_v3(gbuffer *gb, area rect, area clip, int radius, fill_params params) {
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
    painter = (params.fill_type == FILL_TYPE_SOLID) ?
        painter_interface_for_same_color_in_row(&chroma, &alpha) :
        painter_interface_for_different_color_per_pixel(&chroma, &alpha);

    paint_area_v3(&painter, gb, top,    SECTION_TOP,    clip);
    paint_area_v3(&painter, gb, middle, SECTION_CENTER, clip);
    paint_area_v3(&painter, gb, bottom, SECTION_BOTTOM, clip);

    if (radius > 0) {
        alpha = alpha_interface_for_rounded_corners(radius);
        painter = painter_interface_for_different_color_per_pixel(&chroma, &alpha);

        paint_area_v3(&painter, gb, top_left,     SECTION_TOP_LEFT,     clip);
        paint_area_v3(&painter, gb, top_right,    SECTION_TOP_RIGHT,    clip);
        paint_area_v3(&painter, gb, bottom_left,  SECTION_BOTTOM_LEFT,  clip);
        paint_area_v3(&painter, gb, bottom_right, SECTION_BOTTOM_RIGHT, clip);
    }
}
