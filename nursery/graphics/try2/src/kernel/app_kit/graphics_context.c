#include "../memory/malloc.h"
#include "graphics_context.h"


graphics_context_t *new_graphics_context(gbuffer *gb) {
    graphics_context_t *ctx = kmalloc(sizeof(graphics_context_t));
    ctx->buffer = gb;
    ctx->state.origin = point_of(0, 0);
    ctx->state.clip = gb->area;   // full buffer
    ctx->state.fill = color_params_solid(color_black());
    ctx->state.stroke = color_black();
    ctx->stack_top = -1; // the next place to store things.
    return ctx;
}

void gc_free(graphics_context_t *ctx) {
    kfree(ctx);
}

void gfx_push_state(graphics_context_t *ctx) {
    if (ctx->stack_top + 1 >= GFX_STACK_MAX)
        return;
    ctx->stack[++ctx->stack_top] = ctx->state;
}

void gfx_pop_state(graphics_context_t *ctx) {
    if (ctx->stack_top < 0)
        return;
    ctx->state = ctx->stack[--ctx->stack_top];
}

void gfx_clip_to_area(graphics_context_t *ctx, area local_clip) {
    // support transformations first (move, rotate, scale)
    area translated = area_translate(local_clip, ctx->state.origin);

    // each clipping action further restricts what can be drawn
    // a child control will push, restrict, paint, pop.
    ctx->state.clip = area_intersect(ctx->state.clip, translated);
}

// ---------------------------------------

void gfx_move_origin(graphics_context_t *ctx, int dx, int dy) {
    // this gives the location of a view, so that the view can draw in coordinates relative to itself
    ctx->state.origin.x += dx;
    ctx->state.origin.y += dy;
}

void gfx_set_fill(graphics_context_t *ctx, color_params fill) {
    ctx->state.fill = fill;
}
void gfx_set_stroke(graphics_context_t *ctx, color clr, int thickness) {
    ctx->state.stroke = clr;
    ctx->state.thickness = thickness;
}
void gfx_set_roundness(graphics_context_t *ctx, int corner_radius) {
    ctx->state.corner_radius = corner_radius;
}
void gfx_set_shadow(graphics_context_t *ctx, shadow_params shadow) {
    ctx->state.shadow = shadow;
}
void gfx_set_text(graphics_context_t *ctx, text_params text) {
    ctx->state.text = text;
}

// ---------------------------------------

void gfx_draw_rect(graphics_context_t *ctx, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area local_rect = area_translate(rect, ctx->state.origin);
    if (area_is_empty(area_intersect(local_rect, ctx->state.clip)))
        return;

    // now draw it, passing in clip explicitly
    gb_rect(ctx->buffer, local_rect, ctx->state.clip, ctx->state.fill, ctx->state.corner_radius);
}

void gfx_draw_line(graphics_context_t *ctx, point p1, point p2) {
    // support transformations first (move, rotate, scale), check noop
    point p1_translated = point_translate(p1, ctx->state.origin);
    point p2_translated = point_translate(p2, ctx->state.origin);
    if (area_is_empty(area_intersect(area_between(p1, p2), ctx->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    // TO Improve: gb_line(ctx->buffer, p1, p2, ctx->state.clip, ctx->state.stroke, ctx->state.corner_radius);
}

void gfx_draw_border(graphics_context_t *ctx, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area draw = area_translate(rect, ctx->state.origin);
    if (area_is_empty(area_intersect(draw, ctx->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    // TO Improve: gb_border(ctx->buffer, draw, ctx->state.clip, ctx->state.stroke, ctx->state.thickness, ctx->state.corner_radius);
}

void gfx_draw_text(graphics_context_t *ctx, const char *text, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area draw = area_translate(rect, ctx->state.origin);
    if (area_is_empty(area_intersect(draw, ctx->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    // TO Improve: gb_text(ctx->buffer, draw, ctx->state.clip, ctx->state.fill, ctx->state.corner_radius);
}
