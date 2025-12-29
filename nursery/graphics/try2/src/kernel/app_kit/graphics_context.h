#pragma once
#include "../graphics/gbuffer.h"


typedef struct graphics_context graphics_context_t;

struct graphics_context {
    gbuffer *buffer;
};

graphics_context_t *new_graphics_context(gbuffer *gb);