#include "gbuffer_internal.h"

typedef struct chroma_interface chroma_interface_t;
typedef struct {
    color (*resolve)(chroma_interface_t *iface, point p, paint_section_t section);
    union {
        color solid_color;
        struct {
            border_style_t style;
            color base;
            color light;
            color dark;
            int thickness;
        } three_dims_border;
        struct {
            fill_params params;
            vectorf gradient_v;
            float gradient_len_sq;
        } gradient;
    } data;
} chroma_interface_t;

// --------------------------------------------------------

static color _solid_color_chroma_resolver(chroma_interface_t *iface, point p, paint_section_t section) {
    return iface->data.solid_color;
}

static color _three_dims_border_chroma_resolver(chroma_interface_t *iface, point p, paint_section_t section) {
    // find which corner, find what style, find which pixel, then decide which color

    int depth;
    bool faces_the_light;

    switch (section) {
        case SECTION_TOP:          depth = p.y;                   faces_the_light = true;  break;
        case SECTION_BOTTOM:       depth = (height - 1 - p.y);    faces_the_light = false; break;
        case SECTION_LEFT:         depth = p.x;                   faces_the_light = true;  break;
        case SECTION_RIGHT:        depth = (width - 1 - p.x);     faces_the_light = false; break;
        case SECTION_TOP_LEFT:     depth = min(p.x, p.y);         faces_the_light = true;  break;
        case SECTION_BOTTOM_RIGHT: depth = min(w-1-p.x, h-1-p.y); faces_the_light = false; break;
        // etc.
    }

    int is_outside = depth < iface->data.three_dims_border.thickness / 2;
    color light = iface->data.three_dims_border.light;
    color dark = iface->data.three_dims_border.light;

    switch (iface->data.three_dims_border.style) {
        case BORDER_RAISED: return faces_the_light ? light : dark;
        case BORDER_SUNKEN: return faces_the_light ? dark : light;
        case BORDER_RIDGE:  return faces_the_light ? (is_outside ? light : dark) : (is_outside ? dark : light);
        case BORDER_GROOVE: return faces_the_light ? (is_outside ? dark : light) : (is_outside ? light : dark);
    }

    return iface->data.three_dims_border.base;
}

static color _gradient_fill_chroma_resolver(chroma_interface_t *iface, point p, paint_section_t section) {
    // given a pixel, say p, find dot product to find projected distance along gradient_v
    // then, normalize the distance to the gradient color space
    fill_params fp = iface->data.gradient.params;

    vectorf pixel_v = vectorf_from_to(fp.gradient_p1, p);
    float proj_distance = vectorf_dot_product(pixel_v, fp.gradient_v);
    float factor = clamp(proj_distance / fp.gradient_len_sq, 0.0f, 1.0f);
    return color_transition(fp.clr, fp.clr2, fp.ease(factor));
}

// --------------------------------------------------------

static chroma_interface_t chroma_interface_for_solid_color(chroma_interface_t *chroma, color c) {
    return (chroma_interface_t){
        .resolve = _solid_color_chroma_resolver,
        .data = {
            .solid_color = c
        }
    };
}

static chroma_interface_t chroma_interface_for_3d_border(chroma_interface_t *chroma, border_style_t style, color base, factor contrast, int thickness) {

    return (chroma_interface_t){
        .resolve = _three_dims_border_chroma_resolver,
        .data = {
            .three_dims_border = {
                .style = style,
                .base = base,
                .light = color_lighten(base, contrast),
                .dark = color_darken(base, contrast),
                .thickness = thickness,
            }
        }
    };
}

static chroma_interface_t chroma_interface_for_gradient(chroma_interface_t *chroma, fill_params params) {
    vectorf v = vectorf_from_to(params.gradient_p1, params.gradient_p2);
    return (chroma_interface_t){
        .resolve = _gradient_fill_chroma_resolver,
        .data = {
            .gradient = {
                .params = params,
                .gradient_v = v,
                .gradient_len_sq = vectorf_dot_product(v, v),
            }
        }
    };
}
