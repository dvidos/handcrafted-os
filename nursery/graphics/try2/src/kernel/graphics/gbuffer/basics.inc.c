#include "gbuffer_internal.h"


#include "alpha.inc.c"
#include "chroma.inc.c"

// // fast and slow chroma & alpha discovery, as well as color painting
// typedef color (color_resolver_func)(point p, void *context);
// typedef void (span_painter_func)(gbuffer *gb, point p, int width, void *context);


// // -------------------------------------------------------------------
// // fast and slow color resolving (solid / gradient / three-dims / alpha)
// // -------------------------------------------------------------------

// typedef struct solid_color_resolver_context { color solid; } solid_color_resolver_context_t;
// typedef struct gradient_color_resolver_context { fill_params fp; } gradient_color_resolver_context_t;
// typedef struct three_dims_border_color_resolver_context { border_style_t style; color base_color; color dark; color light; int thickness; } three_dims_border_color_resolver_context_t;
// typedef struct rounded_corner_alpha_resolver_context { point center; int radius; int squared_in_boundary; int squared_out_boundary; int squared_bounds_diff; color_resolver_func *chroma_resolver; void *chroma_resolver_context; } rounded_corner_alpha_resolver_context_t;
// typedef struct rounded_border_alpha_resolver_context { point center; int radius; int thickness; int inner_radius_in_boundary_sq; int inner_radius_out_boundary_sq; int inner_radius_sq_diff; int outer_radius_in_boundary_sq; int outer_radius_out_boundary_sq; int outer_radius_sq_diff; color_resolver_func *chroma_resolver; void *chroma_resolver_context; } rounded_border_alpha_resolver_context_t;

// static color solid_color_resolver(point p, void *context) {
//     return ((solid_color_resolver_context_t *)context)->solid;
// }

// static void solid_color_resolver_setup(solid_color_resolver_context_t *ctx, color solid) {
//     ctx->solid = solid;
// }

// static void gradient_color_resolver_setup(gradient_color_resolver_context_t *ctx, fill_params fp) {
//     ctx->fp = fp;
// }

// static color gradient_color_resolver(point p, void *context) {
//     // given a pixel, say p, find dot product to find projected distance along gradient_v
//     // then, normalize the distance to the gradient color space
//     fill_params fp = ((gradient_color_resolver_context_t *)context)->fp;

//     vectorf pixel_v = vectorf_from_to(fp.gradient_p1, p);
//     float proj_distance = vectorf_dot_product(pixel_v, fp.gradient_v);
//     float factor = clamp(proj_distance / fp.gradient_len_sq, 0.0f, 1.0f);
//     return color_transition(fp.clr, fp.clr2, fp.ease(factor));
// }

// void three_dims_border_color_resolver_context_setup(three_dims_border_color_resolver_context_t *ctx, border_style_t style, color base, factor contrast, int thickness) {
//     ctx->style = style;
//     ctx->base_color = base;
//     ctx->light = color_lighten(base, contrast);
//     ctx->dark = color_darken(base, contrast);
//     ctx->thickness = thickness;
// }

// static color three_dims_border_color_resolver(point p, void *context) {
//     three_dims_border_color_resolver_context_t *ctx = (three_dims_border_color_resolver_context_t *)context;
//     // TODO: test this three

//     // do fast x <=> y comparisons to demonstrate a selective color
//     // should be usable with straight borders and with rounded corners.
//     // should support NONE, FLAT, RAISED, SUNKEN, RIDGE, GROOVE
//     // separation might be on the 45o diagonal, 
//     // - and have a bright or dark spot 1-2 pixels on this diagonal, 
//     // - and a 1-2 pixels of medium on the top-right and bottom-left diagonal
    
//     bool is_facing_top_left = (p.x <= p.y);
//     int half_border = ctx->thickness < 2 ? 1 : (ctx->thickness / 2); 
//     bool is_outer = (p.x < half_border) || (p.y < half_border);

//     switch (ctx->style) {
//         case BORDER_RAISED: return is_facing_top_left ? ctx->light : ctx->dark;
//         case BORDER_SUNKEN: return is_facing_top_left ? ctx->dark : ctx->light;
//         case BORDER_RIDGE: 
//             if (is_outer)   return is_facing_top_left ? ctx->light : ctx->dark;
//             else            return is_facing_top_left ? ctx->dark : ctx->light;
//         case BORDER_GROOVE:
//             if (is_outer)   return is_facing_top_left ? ctx->dark : ctx->light;
//             else            return is_facing_top_left ? ctx->light : ctx->dark;
//         case BORDER_FLAT:
//             return ctx->base_color;
//     }

//     return color_black();
// }

// static void rounded_corner_alpha_resolver_setup(rounded_corner_alpha_resolver_context_t *ctx, point center, int radius, color_resolver_func *chroma_resolver, void *chroma_resolver_context) {
//     ctx->center = center;
//     ctx->radius = radius;
//     ctx->squared_in_boundary = (radius - 1) * (radius - 1);
//     ctx->squared_out_boundary = (radius - 0) * (radius - 0);
//     ctx->squared_bounds_diff = ctx->squared_out_boundary - ctx->squared_in_boundary;
//     ctx->chroma_resolver = chroma_resolver;
//     ctx->chroma_resolver_context = chroma_resolver_context;
// }

// static color rounded_corner_alpha_resolver(point p, void *context) {
//     rounded_corner_alpha_resolver_context_t *ctx = (rounded_corner_alpha_resolver_context_t *)context;
//     // TODO: test this

//     int squared_distance = vector_squared_distance(vector_from_to(ctx->center, p));
//     uint8_t alpha;
//     if      (squared_distance <= ctx->squared_in_boundary) alpha = 0xFF;
//     else if (squared_distance >= ctx->squared_out_boundary) alpha = 0;
//     else    alpha = (ctx->squared_out_boundary - squared_distance) * 0xFF / ctx->squared_bounds_diff;

//     color chroma = ctx->chroma_resolver(p, ctx->chroma_resolver_context);
//     return color_with_alpha(alpha, chroma);
// }

// static color rounded_border_alpha_resolver_setup(rounded_border_alpha_resolver_context_t *ctx, point center, int radius, int thickness, color_resolver_func *chroma_resolver, void *chroma_resolver_context) {
//     ctx->center = center;
//     ctx->radius = radius;

//     int inner_radius = radius < thickness ? 0 : radius - thickness;
//     ctx->inner_radius_in_boundary_sq  = (inner_radius - 1) * (inner_radius - 1);
//     ctx->inner_radius_out_boundary_sq = (inner_radius - 0) * (inner_radius - 0);
//     ctx->inner_radius_sq_diff = ctx->inner_radius_out_boundary_sq - ctx->inner_radius_in_boundary_sq;
    
//     int outer_radius = radius;
//     ctx->outer_radius_in_boundary_sq  = (outer_radius - 1) * (outer_radius - 1);
//     ctx->outer_radius_out_boundary_sq = (outer_radius - 0) * (outer_radius - 0);
//     ctx->outer_radius_sq_diff = ctx->outer_radius_out_boundary_sq - ctx->outer_radius_in_boundary_sq;

//     ctx->chroma_resolver = chroma_resolver;
//     ctx->chroma_resolver_context = chroma_resolver_context;
// }

// static color rounded_border_alpha_resolver(point p, void *context) {
//     rounded_border_alpha_resolver_context_t *ctx = (rounded_border_alpha_resolver_context_t *)context;
//     // TODO: test this too

//     int squared_distance = vector_squared_distance(vector_from_to(ctx->center, p));
//     uint8_t alpha = 0;
//     if (squared_distance < ctx->inner_radius_in_boundary_sq)
//         alpha = 0; // inside border

//     else if (squared_distance >= ctx->inner_radius_in_boundary_sq && squared_distance < ctx->inner_radius_out_boundary_sq) 
//         alpha = (squared_distance - ctx->inner_radius_in_boundary_sq) * 0xFF / ctx->inner_radius_sq_diff;

//     else if (squared_distance >= ctx->inner_radius_in_boundary_sq && squared_distance < ctx->outer_radius_in_boundary_sq) 
//         alpha = 0xFF; // on border zone

//     else if (squared_distance >= ctx->outer_radius_in_boundary_sq && squared_distance < ctx->outer_radius_out_boundary_sq) 
//         alpha = (ctx->outer_radius_out_boundary_sq - squared_distance) * 0xFF / ctx->outer_radius_sq_diff;

//     else if (squared_distance > ctx->outer_radius_in_boundary_sq) 
//         alpha = 0; // outsize border

//     color chroma = ctx->chroma_resolver(p, ctx->chroma_resolver_context);
//     return color_with_alpha(alpha, chroma);
// }

// struct resolver_methods {
//     color_resolver_func *solid;
//     color_resolver_func *gradient;
//     color_resolver_func *three_dimensional;
//     color_resolver_func *rounded_corner_alpha;
//     color_resolver_func *rounded_border_alpha;
//     void (*solid_setup)(solid_color_resolver_context_t *ctx, color solid);
//     void (*gradient_setup)(gradient_color_resolver_context_t *ctx, fill_params fp);
//     void (*three_dimentional_setup)(three_dims_border_color_resolver_context_t *ctx, border_style_t style, color base, factor contrast, int thickness);
//     void (*rounded_corner_alpha_setup)(rounded_corner_alpha_resolver_context_t *ctx, point center, int radius, color_resolver_func *chroma_resolver, void *chroma_resolver_context);
//     color (*rounded_border_alpha_setup)(rounded_border_alpha_resolver_context_t *ctx, point center, int radius, int thickness, color_resolver_func *chroma_resolver, void *chroma_resolver_context);
// };

// static struct resolver_methods resolvers = {
//     .solid = solid_color_resolver,
//     .gradient = gradient_color_resolver,
//     .three_dimensional = three_dims_border_color_resolver,
//     .rounded_corner_alpha = rounded_corner_alpha_resolver,
//     .rounded_border_alpha = rounded_border_alpha_resolver,

//     .solid_setup = solid_color_resolver_setup,
//     .gradient_setup = gradient_color_resolver_setup,
//     .three_dimentional_setup = three_dims_border_color_resolver_context_setup,
//     .rounded_corner_alpha_setup = rounded_corner_alpha_resolver_setup,
//     .rounded_border_alpha_setup = rounded_border_alpha_resolver_setup,
// };


// -------------------------------------------------------------------
// fast and slow span (row) painting
// -------------------------------------------------------------------

typedef struct paint_span_fast_solid_replace_context { color c; } paint_span_fast_solid_replace_context_t;
typedef struct paint_span_slow_solid_blend_context { color c; } paint_span_slow_solid_blend_context_t;
typedef struct paint_span_slow_gradient_blend_context { color_resolver_func *color_resolver; void *color_resolver_context; } paint_span_slow_gradient_blend_context_t;

static void paint_span_fast_solid_replace(gbuffer *gb, point p, int width, void *context) {
    // good for solid color, and solid opaqueness
    color c = ((paint_span_fast_solid_replace_context_t *)context)->c;
    uint32_t *pixel = _pixel_ptr(gb, p.x, p.y);
    while (width-- > 0)
        _replace_pixel(*pixel++, c);
}

static void paint_span_slow_solid_blend(gbuffer *gb, point p, int width, void *context) {
    // good for blending of a solid color (e.g. semi transparent or rounded rect)
    color c = ((paint_span_slow_solid_blend_context_t *)context)->c;
    uint32_t *pixel = _pixel_ptr(gb, p.x, p.y);
    while (width-- > 0)
        _blend_pixel(*pixel++, c);
}

static void paint_span_slow_gradient_blend(gbuffer *gb, point p, int width, void *context) {
    // good for blending of a gradient color (e.g. semi transparent or rounded rect)
    // this can ve improved, to avoid recalculating dot product for each pixel.

    color_resolver_func *resolve_color = ((paint_span_slow_gradient_blend_context_t *)context)->color_resolver;
    void *resolve_color_context = ((paint_span_slow_gradient_blend_context_t *)context)->color_resolver_context;
    int x_end = p.x + width;
    uint32_t *pixel = _pixel_ptr(gb, p.x, p.y);
    for (; p.x < x_end; p.x++)
        _blend_pixel(pixel++, resolve_color(p, resolve_color_context));
}



// -------------------------------------------------------------------

typedef enum {
    CORNER_TL,
    CORNER_TR,
    CORNER_BL,
    CORNER_BR
} corner_t;

static inline void area_split_rounded_fill_areas(area rect, int radius, area *top, area *middle, area *bottom, area *top_left, area *top_right, area *bottom_left, area *bottom_right);
static inline void area_split_rounded_border_areas(area rect, int radius, int thickness, area *top, area *bottom, area *left, area *right, area *top_left, area *top_right, area *bottom_left, area *bottom_right);

static inline void paint_area(gbuffer *gb, area part, area clip, span_painter_func *painter, void *painter_ctx) {
    area draw = area_intersect(part, clip);
    if (area_is_empty(draw)) return;

    for (int y = draw.y; y < draw.y + draw.height; y++) {
        point p = point_of(draw.x, y);
        painter(gb, p, draw.width, painter_ctx);
    }
}
static inline void paint_corner(gbuffer *gb, area corner, area clip, corner_t which, span_painter_func *painter, void *painter_ctx) {
    area draw = area_intersect(corner, clip);
    if (area_is_empty(draw)) return;


    for (int target_y = 0; target_y < draw.height; target_y++) {
        int source_y = flip_y ? (radius - 1 - target_y) : target_y;

        point p = point_of(draw.x, draw.y + target_y);
        uint32_t *pixel = _pixel_ptr(gb, p.x, p.y);

        for (int target_X = 0; target_X < draw.width; target_X++) {
            int source_x = flip_x ? (radius - 1 - target_X) : target_X;

            painter(gb, p, draw.width, painter_ctx);
        }
    }
}

// we should be good to implement the various things.
void gb_draw_border_v2(gbuffer *gb, area rect, area clip, int radius, int thickness, border_style_t style, color base_color, factor contrast) {
    if (thickness <= 0 || style == BORDER_NONE || radius < 0 || area_is_empty(rect) || area_is_empty(clip))
        return;
    

    // even for non-rounded corners, corners will be draw in a special way
    // split things areas up
    int corner_size = radius > 0 ? radius : thickness;
    area top, bottom, left, right, top_left, top_right, bottom_left, bottom_right;
    area_split_rounded_border_areas(rect, corner_size, thickness, &top, &bottom, &left, &right, &top_left, &top_right, &bottom_left, &bottom_right);


    // fill color resolver (solid for FLAT style, 3D for all others)
    color_resolver_func *chroma_resolver;
    void *chroma_resolver_ctx;
    if (style == BORDER_FLAT) {
        chroma_resolver = resolvers.solid;
        solid_color_resolver_context_t ctx = { .solid = base_color };
        chroma_resolver_ctx = &ctx;
    } else {
        chroma_resolver = resolvers.three_dimensional;
        three_dims_border_color_resolver_context_t ctx;
        resolvers.three_dimentional_setup(&ctx, style, base_color, contrast, thickness);
        chroma_resolver_ctx = &ctx;
    }

    // painting the flat zones (non-corners)
    span_painter_func *zone_painter;
    void *zone_painter_context;
    if (style == BORDER_FLAT) {
        zone_painter = paint_span_fast_solid_replace;
        paint_span_fast_solid_replace_context_t solid_painter_ctx = { .c = base_color };
        zone_painter_context = &solid_painter_ctx;
    } else {
        zone_painter = paint_span_slow_gradient_blend;
        paint_span_slow_gradient_blend_context_t slow_painter_ctx = { .color_resolver = chroma_resolver, .color_resolver_context = chroma_resolver_ctx };
        zone_painter_context = &slow_painter_ctx;
    }

    paint_area(gb, top, clip, zone_painter, zone_painter_context);
    paint_area(gb, left, clip, zone_painter, zone_painter_context);
    paint_area(gb, right, clip, zone_painter, zone_painter_context);
    paint_area(gb, bottom, clip, zone_painter, zone_painter_context);

    
    // painting the corners, rounded or not
    span_painter_func *corner_painter;
    void *corner_painter_ctx;
    if (style == BORDER_FLAT) {
        corner_painter = paint_span_fast_solid_replace;
        paint_span_fast_solid_replace_context_t fast_solid_painter_ctx = { .c = base_color };
        corner_painter_ctx = &fast_solid_painter_ctx;
    } else {
        corner_painter = paint_span_slow_gradient_blend;
        paint_span_slow_gradient_blend_context_t slow_blend_painter_ctx = { .color_resolver = chroma_resolver, .color_resolver_context = chroma_resolver_ctx };
        corner_painter_ctx = &slow_blend_painter_ctx;
    }

    // see if we need alpha channel or not
    color_resolver_func *corner_color_resolver;
    void *corner_color_resolver_ctx;
    if (radius == 0) {
        corner_color_resolver = chroma_resolver;
        corner_color_resolver_ctx = chroma_resolver_ctx;
    } else {
        corner_color_resolver = resolvers.rounded_border_alpha;
        rounded_border_alpha_resolver_context_t rounded_context;
        resolvers.rounded_border_alpha_setup(&rounded_context, point_zero(), radius, thickness, chroma_resolver, chroma_resolver_ctx);
    }

    // if we have FLAT corners, prepare a tile and flip
    // if we have NON-FLAT 3d corners, we have to draw each individually
    // either the painter, or the paint_corner() function must know which corner we are painting, as they are all different.
    // so, maybe let's accept this.
    // let's do the simplest thing, we can optimize later

    // ???

    paint_area(gb, top_left, clip, corner_painter, corner_painter_ctx);
    paint_area(gb, top_right, clip, corner_painter, corner_painter_ctx);
    paint_area(gb, bottom_left, clip, corner_painter, corner_painter_ctx);
    paint_area(gb, bottom_right, clip, corner_painter, corner_painter_ctx);
}


void gb_draw_border_v3(gbuffer *gb, area rect, area clip, int radius, int thickness, border_style_t style, color base_color, factor contrast) {
    areas = area_split()

    color_selector = ...
    painter = ...
    paint_solid_border(top)
    paint_solid_border(left)
    paint_solid_border(right)
    paint_solid_border(bottom)

    color_selector = ...
    painter = ...
    paint_corner(CORNER_TL);
    paint_corner(CORNER_TR);
    paint_corner(CORNER_BL);
    paint_corner(CORNER_BR);
}
void gb_fill_rect_v3(gbuffer *gb, area rect, area clip, int radius, fill_params params) {
    areas = area_split()

    color_selector = ...
    painter = ...
    paint_solid_border(top)
    paint_solid_border(middle)
    paint_solid_border(bottom)

    color_selector = ...
    painter = ...
    paint_corner(CORNER_TL);
    paint_corner(CORNER_TR);
    paint_corner(CORNER_BL);
    paint_corner(CORNER_BR);
}











// --------------------------------------------------
// example
// --------------------------------------------------

void draw_rounded_rect_with_3d_border(gbuffer *gb, area rect, int radius, int border_width, color fill_color, color light_color, color dark_color) {
    // -----------------------------
    // 1) Setup the fill resolver
    // -----------------------------
    struct rounded_corner_alpha_resolver_context fill_ctx = {
        .center = point_of(rect.x + radius, rect.y + radius), // will be mirrored for other corners
        .radius = radius,
        .border_width = 0,
        .fill_color = fill_color,
    };
    
    color_resolver_func *fill_resolver = rounded_corner_resolver; 

    // -----------------------------
    // 2) Setup the painter for fill
    // -----------------------------
    struct paint_span_slow_gradient_blend_context fill_painter_ctx = {
        .color_resolver = fill_resolver,
        .color_resolver_context = &fill_ctx
    };
    
    span_painter_func *fill_painter = paint_span_slow_gradient_blend;

    // -----------------------------
    // 3) Draw geometry for rounded fill
    // -----------------------------
    for (int dy = 0; dy < radius; dy++) {
        int span_len = radius - dy; // simple quarter circle
        point p_top_left = point_of(rect.x + radius - span_len, rect.y + dy);
        fill_painter(gb, p_top_left, span_len, &fill_painter_ctx);

        // Mirror for other corners
        point p_top_right = point_of(rect.x + rect.width - radius, rect.y + dy);
        fill_painter(gb, p_top_right, span_len, &fill_painter_ctx);

        point p_bottom_left = point_of(rect.x + radius - span_len, rect.y + rect.height - radius + dy);
        fill_painter(gb, p_bottom_left, span_len, &fill_painter_ctx);

        point p_bottom_right = point_of(rect.x + rect.width - radius, rect.y + rect.height - radius + dy);
        fill_painter(gb, p_bottom_right, span_len, &fill_painter_ctx);
    }

    // -----------------------------
    // 4) Setup the border resolver for 3D raised effect
    // -----------------------------
    struct rounded_border_alpha_resolver_context border_ctx = {
        .rect_topleft = point_of(rect.x, rect.y),
        .rect_bottomright = point_of(rect.x + rect.width, rect.y + rect.height),
        .radius = radius,
        .border_width = border_width,
        .color_top = light_color,
        .color_bottom = dark_color
    };

    color_resolver_func *border_resolver = rounded_border_resolver;

    struct paint_span_slow_gradient_blend_context border_painter_ctx = {
        .color_resolver = border_resolver,
        .color_resolver_context = &border_ctx
    };

    span_painter_func *border_painter = paint_span_slow_gradient_blend;

    // -----------------------------
    // 5) Draw geometry for the border
    // -----------------------------
    // Draw top and left sides (light)
    for (int i = 0; i < border_width; i++) {
        point top = point_of(rect.x, rect.y + i);
        border_painter(gb, top, rect.width, &border_painter_ctx);

        point left = point_of(rect.x + i, rect.y + radius);
        border_painter(gb, left, rect.height - 2 * radius, &border_painter_ctx);
    }

    // Draw bottom and right sides (dark)
    for (int i = 0; i < border_width; i++) {
        point bottom = point_of(rect.x, rect.y + rect.height - 1 - i);
        border_painter(gb, bottom, rect.width, &border_painter_ctx);

        point right = point_of(rect.x + rect.width - 1 - i, rect.y + radius);
        border_painter(gb, right, rect.height - 2 * radius, &border_painter_ctx);
    }

    // -----------------------------
    // 6) Corners handled inside resolvers via mirrored spans
    // -----------------------------
}













