#pragma once
#include <stdbool.h>
#include "../fundamentals.h"
#include "../graphics/geometry.h"
#include "../graphics/gbuffer.h"
#include "graphics_context.h"
#include "../concepts/events.h"
#include "view.h"


typedef enum {
    SURFACE_DESKTOP,
    SURFACE_WINDOW,
    SURFACE_PANEL,
    SURFACE_OVERLAY,
    SURFACE_CURSOR
} surface_role_t;

typedef struct surface surface_t;

typedef struct surface_owner_interface_t {
    void (*surface_invalidated)(void *surface, area dirty);
} surface_owner_interface_t;


typedef struct surface_callbacks {
    // when screen manager asks the view to paint, when it's dirty
    void (*paint)(surface_t *s, graphics_context_t *gc, area dirty);

    // for handling events
    void (*on_key_event)(surface_t *s, key_event_t e);
    void (*on_mouse_event)(surface_t *s, mouse_event_t e);

    // for when keyboard focus is set/pushed/changed
    void (*on_focus_gained)(surface_t *);
    void (*on_focus_lost)(surface_t *);

} surface_callbacks_t;

// represents a positioned graphics buffer on screen
// adds position, flags, callbacks
struct surface {
    area frame;                  // position + size in screen coordinates
    gbuffer *buffer;             // drawing, owned by surface

    surface_role_t role;         // composition
    int is_visible;
    int is_opaque;               // hint for composition optimization

    area dirty_area;             // dirty area is local to surface, after paint(), it is cleared by screen manager after redrawing
    bool needs_redraw;           // similar to dirty area. set to flag redraw, cleared by screen manager after redrawing

    bool accepts_mouse;
    bool accepts_keyboard;
    surface_callbacks_t callbacks;

    view_t *root_view;
    view_t *focused_view;

    // intrusive list (managed by ScreenManager)
    struct surface *prev; // upper
    struct surface *next; // lower

    surface_owner_interface_t *owner_interface;
};


surface_t *new_surface(int w, int h, surface_role_t role);
void surface_destroy(surface_t *s);

void surface_set_position(surface_t *s, int x, int y);
void surface_set_size(surface_t *s, int w, int h);   // realloc buffer
void surface_show(surface_t *s);
void surface_hide(surface_t *s);
void surface_mark_clean(surface_t *s);
void surface_damage_area(surface_t *s, area area);
void surface_damage_all(surface_t *s);
void surface_begin_draw(surface_t *s, graphics_context_t *gc);
void surface_end_draw(surface_t *s);
void surface_raise(surface_t *s);
void surface_lower(surface_t *s);
void surface_set_z(surface_t *s, int z);
void surface_handle_key(surface_t *s, key_event_t e);
void surface_add_view(surface_t *s, view_t *v);
void surface_set_focused_view(surface_t *s, view_t *v);
void surface_clear_focused_view(surface_t *s);