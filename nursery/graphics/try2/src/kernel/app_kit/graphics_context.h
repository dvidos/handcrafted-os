#pragma once
#include "../graphics/gbuffer.h"


typedef struct graphics_context graphics_context_t;

#define GFX_STACK_MAX 16
typedef struct gfx_state {
    area clip;        // clip in buffer coordinates
    point origin;
    color_params fill;
    color stroke;
    int thickness;
    int corner_radius;
    shadow_params shadow;
    text_params text;
} gfx_state;

struct graphics_context {
    gbuffer *buffer;

    gfx_state stack[GFX_STACK_MAX];
    int stack_top;

    gfx_state state;
};

graphics_context_t *new_graphics_context(gbuffer *gb);
void gc_free(graphics_context_t *ctx);

void gfx_push_state(graphics_context_t *ctx);
void gfx_pop_state(graphics_context_t *ctx);

void gfx_clip_to_area(graphics_context_t *ctx, area local_clip);
void gfx_move_origin(graphics_context_t *ctx, int dx, int dy);
void gfx_set_fill(graphics_context_t *ctx, color_params fill);
void gfx_set_stroke(graphics_context_t *ctx, color clr, int thickness);
void gfx_set_roundness(graphics_context_t *ctx, int corner_radius);
void gfx_set_shadow(graphics_context_t *ctx, shadow_params shadow);
void gfx_set_text(graphics_context_t *ctx, text_params text);

void gfx_draw_rect(graphics_context_t *ctx, area rect);
void gfx_draw_line(graphics_context_t *ctx, point p1, point p2);
void gfx_draw_border(graphics_context_t *ctx, area rect);
void gfx_draw_text(graphics_context_t *ctx, const char *text, area rect);