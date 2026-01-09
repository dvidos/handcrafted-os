#pragma once
#include <stdbool.h>
#include "../fundamentals.h"
#include "../graphics/geometry.h"
#include "../graphics/gbuffer.h"
#include "graphics_context.h"
#include "../concepts/events.h"
#include "view.h"
#include "../containers/dllist.h"



typedef enum {
    SURFACE_DESKTOP,
    SURFACE_WINDOW,
    SURFACE_PANEL,
    SURFACE_OVERLAY,
    SURFACE_CURSOR
} surface_role_t;

typedef struct surface surface_t;

typedef struct surface_owner_interface_t {
    void (*surface_invalidated)(surface_t *surface, area dirty);
} surface_owner_interface_t;

typedef void (surface_paint_func)(surface_t *s, graphics_context_t *gc, area dirty);


int surface_get_dlist_node_offset();
surface_t *new_surface(int w, int h, surface_role_t role, bool focusable, const char *debug_info);
void surface_destroy(surface_t *s);

const char *surface_get_debug_info(surface_t *s);
area surface_get_frame(surface_t *s);
size surface_get_size(surface_t *s);
point surface_get_location(surface_t *s);
gbuffer *surface_get_buffer(surface_t *s);
area surface_get_dirty_area(surface_t *s);

bool surface_is_focusable(surface_t *s);
bool surface_is_visible(surface_t *s);
bool surface_is_opaque(surface_t *s);
bool surface_needs_redraw(surface_t *s);
bool surface_accepts_mouse(surface_t *s);
void surface_mark_clean(surface_t *s);

void surface_set_position(surface_t *s, int x, int y);
void surface_set_size(surface_t *s, int w, int h);   // realloc buffer
void surface_show(surface_t *s);
void surface_hide(surface_t *s);
void surface_mark_clean(surface_t *s);
void surface_invalidate_area(surface_t *s, area area);
void surface_invalidate_all(surface_t *s);
void surface_raise(surface_t *s);
void surface_lower(surface_t *s);
void surface_set_z(surface_t *s, int z);
void surface_handle_key(surface_t *s, key_event_t e);
void surface_add_view(surface_t *s, view_t *v);
void surface_set_focused_view(surface_t *s, view_t *v);

void surface_set_owner_interface(surface_t *s, surface_owner_interface_t *interface);
void surface_set_on_paint_behavior(surface_t *s, surface_paint_func *behavior);

void surface_on_paint(surface_t *s, graphics_context_t *gc, area dirty);
void surface_on_key_event(surface_t *s, key_event_t e);
void surface_on_mouse_event(surface_t *s, mouse_event_t e);
void surface_on_focus_gained(surface_t *s);
void surface_on_focus_lost(surface_t *s);
void surface_on_shown(surface_t *s);
void surface_on_hidden(surface_t *s);

void surface_log_debug_info(surface_t *s, const char *prefix);
