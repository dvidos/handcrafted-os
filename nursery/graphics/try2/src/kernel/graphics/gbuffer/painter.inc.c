#pragma once
#include "gbuffer_internal.h"
#include "alpha.inc.c"
#include "chroma.inc.c"

typedef struct painter_interface painter_interface_t;
struct painter_interface {
    void (*paint)(painter_interface_t *iface, gbuffer *gb, area resolver_area, point paint_start, int width, paint_section_t section);
    struct {
        chroma_interface_t *chroma;
        alpha_interface_t *alpha;
    } data;
};

// -----------------------------------------------------

static void _same_opaque_color_in_row_painter(painter_interface_t *iface, gbuffer *gb, area resolver_area, point paint_start, int width, paint_section_t section) {
    color same_opaque_color = color_with_alpha(
        iface->data.alpha->resolve(iface->data.alpha, paint_start, section),
        iface->data.chroma->resolve(iface->data.chroma, paint_start, section)
    );
    uint32_t *pixel = _pixel_ptr(gb, paint_start.x, paint_start.y);
    while (width-- > 0)
        _replace_pixel(pixel++, same_opaque_color);
}

static void _different_color_per_pixel_painter(painter_interface_t *iface, gbuffer *gb, area resolver_area, point paint_start, int width, paint_section_t section) {
    uint32_t *pixel = _pixel_ptr(gb, paint_start.x, paint_start.y);
    point resolver_point = point_of(paint_start.x - resolver_area.x, paint_start.y - resolver_area.y);
    while (width-- > 0) {
        _blend_pixel(pixel, color_with_alpha(
            iface->data.alpha->resolve(iface->data.alpha, resolver_point, section),
            iface->data.chroma->resolve(iface->data.chroma, resolver_point, section)
        ));
        pixel++;
        resolver_point.x++;
    }
}

static void _alpha_heat_map_painter(painter_interface_t *iface, gbuffer *gb, area resolver_area, point paint_start, int width, paint_section_t section) {
    uint32_t *pixel = _pixel_ptr(gb, paint_start.x, paint_start.y);
    point resolver_point = point_of(paint_start.x - resolver_area.x, paint_start.y - resolver_area.y);
    while (width-- > 0) {
        _replace_pixel(pixel, color_alpha_to_heatmap(iface->data.alpha->resolve(iface->data.alpha, resolver_point, section)));
        pixel++;
        resolver_point.x++;
    }
}

// -----------------------------------------------------

static painter_interface_t painter_interface_for_same_color_in_row(chroma_interface_t *chroma, alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _same_opaque_color_in_row_painter,
        .data = {
            .chroma = chroma,
            .alpha = alpha
        }
    };
}

static painter_interface_t painter_interface_for_different_color_per_pixel(chroma_interface_t *chroma, alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _different_color_per_pixel_painter,
        .data = {
            .chroma = chroma,
            .alpha = alpha
        }
    };
}

static painter_interface_t painter_interface_for_alpha_heat_map(alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _alpha_heat_map_painter,
        .data = {
            .alpha = alpha
        }
    };
}
