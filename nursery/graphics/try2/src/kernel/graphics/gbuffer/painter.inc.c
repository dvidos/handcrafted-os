#pragma once
#include "gbuffer_internal.h"
#include "alpha.inc.c"
#include "chroma.inc.c"

typedef struct painter_interface painter_interface_t;
struct painter_interface {
    void (*paint)(painter_interface_t *iface, gbuffer *gb, point start, int width, paint_section_t section);
    struct {
        chroma_interface_t *chroma;
        alpha_interface_t *alpha;
    } data;
};

// -----------------------------------------------------

static void _same_opaque_color_in_row_painter(painter_interface_t *iface, gbuffer *gb, point start, int width, paint_section_t section) {
    color same_opaque_color = color_with_alpha(
        iface->data.alpha->resolve(iface->data.alpha, start, section),
        iface->data.chroma->resolve(iface->data.chroma, start, section)
    );
    uint32_t *pixel = _pixel_ptr(gb, start.x, start.y);
    while (width-- > 0)
        _replace_pixel(pixel++, same_opaque_color);
}

static void _different_color_per_pixel_painter(painter_interface_t *iface, gbuffer *gb, point start, int width, paint_section_t section) {
    uint32_t *pixel = _pixel_ptr(gb, start.x, start.y);
    int x_end = start.x + width;
    for (; start.x < x_end; start.x++) {
        _blend_pixel(pixel++, color_with_alpha(
            iface->data.alpha->resolve(iface->data.alpha, start, section),
            iface->data.chroma->resolve(iface->data.chroma, start, section)
        ));
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
