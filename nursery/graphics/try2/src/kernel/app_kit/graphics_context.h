#pragma once
#include "../graphics/gbuffer.h"
#include "../graphics/icons/icon32.h"


typedef struct graphics_context graphics_context_t;

#define GFX_STACK_MAX 16
typedef struct gfx_state {
    area clip;        // clip in buffer coordinates
    color stroke;
    point origin;
    fill_params fill;
    border_params border;
    text_params text;
    shadow_params shadow;
} gfx_state;

struct graphics_context {
    gbuffer *buffer;

    gfx_state stack[GFX_STACK_MAX];
    int stack_count;

    gfx_state state;
};

graphics_context_t *new_graphics_context(gbuffer *gb);
void gc_free(graphics_context_t *gc);

void gc_push_state(graphics_context_t *gc);
void gc_pop_state(graphics_context_t *gc);

void gc_clip_to_area(graphics_context_t *gc, area local_clip);
void gc_move_origin(graphics_context_t *gc, int dx, int dy);
void gc_set_fill(graphics_context_t *gc, fill_params fill);
void gc_set_border(graphics_context_t *gc, border_style_t style, color clr, int thickness, float contrast_3d);
void gc_set_roundness(graphics_context_t *gc, int corner_radius);
void gc_set_shadow(graphics_context_t *gc, shadow_params shadow);
void gc_set_text(graphics_context_t *gc, text_params text);

void gc_draw_rect(graphics_context_t *gc, area rect);
void gc_draw_line(graphics_context_t *gc, point p1, point p2);
void gc_draw_border(graphics_context_t *gc, area rect);
void gc_draw_text(graphics_context_t *gc, const char *text, area rect);
void gc_draw_icon(graphics_context_t *gc, const icon32 *icon, area rect);
