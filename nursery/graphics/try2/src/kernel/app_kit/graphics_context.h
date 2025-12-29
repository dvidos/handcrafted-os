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
