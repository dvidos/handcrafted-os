#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "../graphics/geometry.h"
#include "../graphics/gbuffer.h"
#include "graphics_context.h"


typedef enum {
    SURFACE_DESKTOP,
    SURFACE_WINDOW,
    SURFACE_PANEL,
    SURFACE_OVERLAY,
    SURFACE_CURSOR
} surface_role_t;

typedef struct surface surface_t;

typedef void (surface_paint_func)(surface_t *s, graphics_context_t *ctx, area dirty);


// represents a positioned graphics buffer on screen
// adds position, z-index, flags
struct surface {
    area frame;                  // position + size in screen coordinates
    gbuffer *buffer;             // drawing, owned by surface

    surface_role_t role;         // composition
    int z_order;                 // relative ordering
    int is_visible;
    int is_opaque;               // hint for composition optimization

    area dirty_area;             // dirty area is local to surface, after paint(), it is cleared by screen manager after redrawing
    bool needs_redraw;           // similar to dirty area. set to flag redraw, cleared by screen manager after redrawing

    surface_paint_func *paint;   // may be null
    void *owner;                 // can be WM, SM, window, system UI, etc.

    // intrusive list (managed by ScreenManager)
    struct surface *prev;
    struct surface *next;
};


surface_t *new_surface(int w, int h, surface_role_t role);
void surface_destroy(surface_t *s);

void surface_set_position(surface_t *s, int x, int y);
void surface_set_size(surface_t *s, int w, int h);   // realloc buffer
void surface_show(surface_t *s);
void surface_hide(surface_t *s);
void surface_damage_area(surface_t *s, area area);
void surface_damage_all(surface_t *s);
void surface_begin_draw(surface_t *s, graphics_context_t *ctx);
void surface_end_draw(surface_t *s);
void surface_raise(surface_t *s);
void surface_lower(surface_t *s);
void surface_set_z(surface_t *s, int z);
