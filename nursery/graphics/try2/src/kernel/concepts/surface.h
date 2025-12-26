#pragma once
#include "../graphics/gbuffer.h"

// intermediate structure that represents the position of a gbuffer on screen
// add position, z-index, flags, to compose a whole screen
typedef struct surface {
    int x, y;
    int w, h;
    gbuffer *gbuffer;
    int is_visible;
    int z_index;           // 0=back, high=front
    int is_dirty;
    // rect_t dirty;       // surface-local dirty region
    // uint32_t flags;     // desktop, popup, always_on_top, etc.

    void (*draw)(gbuffer *gb, void *draw_data);
    void *draw_data;
} surface_t;
