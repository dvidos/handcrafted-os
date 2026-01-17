#pragma once
#include "gbuffer_internal.h"
#include "alpha.inc.c"
#include "chroma.inc.c"

typedef struct painter_interface painter_interface_t;
struct painter_interface {
    void (*paint)(painter_interface_t *iface, point paint_start, int width);
    void (*prepare)(painter_interface_t *iface, area section_area, paint_section_t section);
    struct {
        gbuffer *gb;
        area section_area;
        paint_section_t section;
        chroma_interface_t *chroma;
        alpha_interface_t *alpha;
    } data;
};

// -----------------------------------------------------

static void _same_opaque_color_in_row_painter(painter_interface_t *iface, point paint_start, int width) {
    color same_opaque_color = color_with_alpha(
        iface->data.alpha->resolve(iface->data.alpha, paint_start),
        iface->data.chroma->resolve(iface->data.chroma, paint_start)
    );
    uint32_t *pixel = _pixel_ptr(iface->data.gb, paint_start.x, paint_start.y);
    while (width-- > 0)
        _replace_pixel(pixel++, same_opaque_color);
}

static void _different_color_per_pixel_painter(painter_interface_t *iface, point paint_start, int width) {
    uint32_t *pixel = _pixel_ptr(iface->data.gb, paint_start.x, paint_start.y);
    point resolver_point = point_of(paint_start.x - iface->data.section_area.x, paint_start.y - iface->data.section_area.y);
    while (width-- > 0) {
        _blend_pixel(pixel, color_with_alpha(
            iface->data.alpha->resolve(iface->data.alpha, resolver_point),
            iface->data.chroma->resolve(iface->data.chroma, resolver_point)
        ));
        pixel++;
        resolver_point.x++;
    }
}

static void _alpha_heat_map_painter(painter_interface_t *iface, point paint_start, int width) {
    uint32_t *pixel = _pixel_ptr(iface->data.gb, paint_start.x, paint_start.y);
    point resolver_point = point_of(paint_start.x - iface->data.section_area.x, paint_start.y - iface->data.section_area.y);
    while (width-- > 0) {
        _replace_pixel(pixel, color_alpha_to_heatmap(iface->data.alpha->resolve(iface->data.alpha, resolver_point)));
        pixel++;
        resolver_point.x++;
    }
}

static void _painter_prepare(painter_interface_t *iface, area section_area, paint_section_t section) {
    iface->data.section_area = section_area;
    iface->data.section = section;
    iface->data.alpha->prepare(iface->data.alpha, section_area, section);
    iface->data.chroma->prepare(iface->data.chroma, section_area, section);
}

// -----------------------------------------------------

static painter_interface_t painter_interface_for_same_color_in_row(gbuffer *gb, chroma_interface_t *chroma, alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _same_opaque_color_in_row_painter,
        .prepare = _painter_prepare,
        .data = {
            .gb = gb,
            .chroma = chroma,
            .alpha = alpha
        }
    };
}

static painter_interface_t painter_interface_for_different_color_per_pixel(gbuffer *gb, chroma_interface_t *chroma, alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _different_color_per_pixel_painter,
        .prepare = _painter_prepare,
        .data = {
            .gb = gb,
            .chroma = chroma,
            .alpha = alpha
        }
    };
}

static painter_interface_t painter_interface_for_alpha_heat_map(gbuffer *gb, alpha_interface_t *alpha) {
    return (painter_interface_t){
        .paint = _alpha_heat_map_painter,
        .prepare = _painter_prepare,
        .data = {
            .gb = gb,
            .alpha = alpha
        }
    };
}
