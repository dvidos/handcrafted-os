#include "../memory/malloc.h"
#include "graphics_context.h"


graphics_context_t *new_graphics_context(gbuffer *gb) {
    graphics_context_t *gc = kmalloc(sizeof(graphics_context_t));
    memset(gc, 0, sizeof(graphics_context_t));
    gc->buffer = gb;
    gc->state.origin = point_of(0, 0);
    gc->state.clip = gb->area;   // full buffer
    gc->state.fill = fill_params_solid(color_black());
    gc->state.stroke = color_black();
    return gc;
}

void gc_free(graphics_context_t *gc) {
    kfree(gc);
}

void gc_push_state(graphics_context_t *gc) {
    if (gc->stack_count >= GFX_STACK_MAX)
        return;
    gc->stack[gc->stack_count++] = gc->state;
}

void gc_pop_state(graphics_context_t *gc) {
    if (gc->stack_count <= 0)
        return;
    gc->state = gc->stack[--gc->stack_count];
}

void gc_clip_to_area(graphics_context_t *gc, area local_clip) {
    // support transformations first (move, rotate, scale)
    area translated = area_translate(local_clip, gc->state.origin);

    // each clipping action further restricts what can be drawn
    // a child control will push, restrict, paint, pop.
    gc->state.clip = area_intersect(gc->state.clip, translated);
}

// ---------------------------------------

void gc_move_origin(graphics_context_t *gc, int dx, int dy) {
    // this gives the location of a view, so that the view can draw in coordinates relative to itself
    gc->state.origin.x += dx;
    gc->state.origin.y += dy;
}

void gc_set_fill(graphics_context_t *gc, fill_params fill) {
    gc->state.fill = fill;
}
void gc_set_border(graphics_context_t *gc, border_style_t style, color clr, int thickness, float contrast_3d) {
    gc->state.border.style = style;
    gc->state.border.clr = clr;
    gc->state.border.thickness = thickness;
    gc->state.border.contrast_3d = contrast_3d;
}
void gc_set_roundness(graphics_context_t *gc, int corner_radius) {
    gc->state.border.radius = corner_radius;
}
void gc_set_shadow(graphics_context_t *gc, shadow_params shadow) {
    gc->state.shadow = shadow;
}
void gc_set_text(graphics_context_t *gc, text_params text) {
    gc->state.text = text;
}

// ---------------------------------------

void gc_draw_rect(graphics_context_t *gc, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area local_rect = area_translate(rect, gc->state.origin);
    if (area_is_empty(area_intersect(local_rect, gc->state.clip)))
        return;

    // now draw it, passing in clip explicitly
    // gb_rect(gc->buffer, local_rect, gc->state.clip, gc->state.fill, gc->state.corner_radius);
    gb_fill_rect_v3(gc->buffer, local_rect, gc->state.clip, gc->state.border.radius, gc->state.fill, gc->state.show_sections, gc->state.show_alpha);
}

void gc_draw_line(graphics_context_t *gc, point p1, point p2) {
    // support transformations first (move, rotate, scale), check noop
    point p1_translated = point_translate(p1, gc->state.origin);
    point p2_translated = point_translate(p2, gc->state.origin);
    if (area_is_empty(area_intersect(area_between(p1, p2), gc->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    // TO Improve: gb_line(gc->buffer, p1, p2, gc->state.clip, gc->state.stroke, gc->state.corner_radius);
}

void gc_draw_border(graphics_context_t *gc, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area draw = area_translate(rect, gc->state.origin);
    if (area_is_empty(area_intersect(draw, gc->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    // gb_border(gc->buffer, draw, gc->state.clip, gc->state.corner_radius, gc->state.thickness, gc->state.stroke);
    gb_draw_border_v3(gc->buffer, draw, gc->state.clip, gc->state.border, gc->state.show_sections, gc->state.show_alpha);
}

void gc_draw_text(graphics_context_t *gc, const char *text, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area draw = area_translate(rect, gc->state.origin);
    if (area_is_empty(area_intersect(draw, gc->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    gb_text(gc->buffer, draw, gc->state.clip, text, gc->state.text);
}

void gc_draw_icon(graphics_context_t *gc, const icon32 *icon, area rect) {
    // support transformations first (move, rotate, scale), check noop
    area draw = area_translate(rect, gc->state.origin);
    if (area_is_empty(area_intersect(draw, gc->state.clip)))
        return;

    // now draw it, but pass clipping in explicitly
    gb_icon(gc->buffer, draw, gc->state.clip, icon, gc->state.stroke);
}

void gc_show_sections(graphics_context_t *gc, bool enabled) {
    gc->state.show_sections = enabled;
}

void gc_show_alpha(graphics_context_t *gc, bool enabled) {
    gc->state.show_alpha = enabled;
}
