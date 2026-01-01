#include "surface.h"
#include "../memory/malloc.h"
#include "../containers/dllist.h"

static view_owner_interface_t view_surface_functions;

surface_t *new_surface(int w, int h, surface_role_t role) {
    surface_t *s = kmalloc(sizeof(surface_t));
    memset(s, 0, sizeof(*s));

    s->role         = role;
    s->frame        = area_of(0, 0, w, h);
    s->dirty_area   = area_of(0, 0, w, h);
    s->needs_redraw = true;

    s->is_visible = true;
    s->is_opaque  = true;

    s->buffer = new_gbuffer(w, h);

    s->root_view = new_base_view();
    view_set_owner_interface(s->root_view, &view_surface_functions, s);
    s->focused_view = NULL;

    return s;
}

void surface_destroy(surface_t *s) {
    if (!s) return;
    gb_free(s->buffer);
    kfree(s);
}

void surface_set_position(surface_t *s, int x, int y) {
    if (s->frame.x == x && s->frame.y == y) return;

    s->frame.x = x;
    s->frame.y = y;
    s->needs_redraw = true;
}

void surface_set_size(surface_t *s, int w, int h) {
    if (s->frame.width == w && s->frame.height == h) return;

    gbuffer *oldb = s->buffer;
    gbuffer *newb = new_gbuffer(w, h);

    int copy_w = min(oldb->area.width,  w);
    int copy_h = min(oldb->area.height, h);

    gb_copy_area_fast(newb, oldb, size_of(copy_w, copy_h), point_zero(), point_zero());
    gb_free(oldb);

    s->buffer = newb;
    s->frame.width  = w;
    s->frame.height = h;
    s->dirty_area = area_of(0, 0, w, h);
    s->needs_redraw = true;
}

void surface_show(surface_t *s) { 
    if (s->is_visible) return;

    s->is_visible = true;
    s->needs_redraw = true;
}

void surface_hide(surface_t *s) { 
    if (!s->is_visible) return;

    s->is_visible = false;
    s->needs_redraw = true;
}

void surface_mark_clean(surface_t *s) {
    s->dirty_area = area_zero();
    s->needs_redraw = false;
}

void surface_damage_area(surface_t *s, area a) {
    if (area_is_empty(a)) return;

    // damage area is local to surface, extend current area
    s->dirty_area = area_union(s->dirty_area, a); 
    s->needs_redraw = true;
}

void surface_damage_all(surface_t *s) {
    s->dirty_area = s->frame;
    s->needs_redraw = true;
}

void surface_begin_draw(surface_t *s, graphics_context_t *gc) {
    // gc is assumed already bound to s->buffer
    // surface only resets damage bookkeeping
}

void surface_end_draw(surface_t *s) {
    // nothing else to do here
    // SM/WM will consume dirty_area + needs_redraw
}

void surface_handle_key(surface_t *s, key_event_t e) {
    if (s->focused_view && s->focused_view->callbacks->on_key_event)
        s->focused_view->callbacks->on_key_event(s->focused_view, e);
}

void surface_add_view(surface_t *s, view_t *v) {
    view_add_child_view(s->root_view, v);
}

void surface_set_focused_view(surface_t *s, view_t *v) {
    if (s->focused_view == v)
        return;

    surface_clear_focused_view(s);

    s->focused_view = v;
    if (v && v->callbacks->on_focus_gained)
        v->callbacks->on_focus_gained(v);
}

void surface_clear_focused_view(surface_t *s) {
    if (s->focused_view && s->focused_view->callbacks->on_focus_lost)
        s->focused_view->callbacks->on_focus_lost(s->focused_view);
    s->focused_view = NULL;
}

// ------------------------------------------------------------

static void _surface_mark_area_dirty(void *owner_data, area dirty) {
    surface_t *s = (surface_t *)owner_data;
    surface_damage_area(s, dirty);
}
static void _surface_request_focus(void *owner_data, view_t *v) {
    surface_t *s = (surface_t *)owner_data;
    surface_set_focused_view(s, v);
}
static void _surface_release_focus(void *owner_data, view_t *v) {
    surface_t *s = (surface_t *)owner_data;
    surface_clear_focused_view(s);
}

static view_owner_interface_t view_surface_functions = {
    .mark_area_dirty = _surface_mark_area_dirty,
    .request_focus = _surface_request_focus,
    .release_focus = _surface_release_focus
};

