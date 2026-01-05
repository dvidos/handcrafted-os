#include "../concepts/logger.h"
#include "surface.h"
#include "../memory/malloc.h"
#include "../containers/dllist.h"
#include "views/background_view.h"

static void _surface_mark_area_dirty(void *owner_data, area dirty);
static void _surface_request_focus(void *owner_data, view_t *v);
static void _surface_release_focus(void *owner_data, view_t *v);

static view_owner_interface_t surface_functions_for_views = {
    .mark_area_dirty = _surface_mark_area_dirty,
    .request_focus = _surface_request_focus,
    .release_focus = _surface_release_focus
};

// -----------------------------------------------------------------------

int surface_get_dlist_node_offset() {
    return offsetof(surface_t, dlist_node);
}

surface_t *new_surface(int w, int h, surface_role_t role) {
    surface_t *s = kmalloc(sizeof(surface_t));
    memset(s, 0, sizeof(*s));

    s->role         = role;
    s->owner_interface = NULL;
    s->frame        = area_of(0, 0, w, h);
    s->dirty_area   = area_of(0, 0, w, h);
    s->needs_redraw = true;

    s->is_visible = true;
    s->is_opaque  = true;

    s->buffer = new_gbuffer(w, h);

    s->root_view = (view_t *)new_background_view();
    view_set_frame(s->root_view, s->frame);
    view_set_owner_interface(s->root_view, &surface_functions_for_views, s);
    s->focused_view = NULL;

    s->focusable = true;
    s->accepts_mouse = true;

    return s;
}

void surface_destroy(surface_t *s) {
    if (!s) return;
    gb_free(s->buffer);
    kfree(s);
}

area surface_get_frame(surface_t *s) {
    return s->frame;
}

size surface_get_size(surface_t *s) {
    return size_of(s->frame.width, s->frame.height);
}

point surface_get_location(surface_t *s) {
    return point_of(s->frame.x, s->frame.y);
}

gbuffer *surface_get_buffer(surface_t *s) {
    return s->buffer;
}

area surface_get_dirty_area(surface_t *s) {
    return s->dirty_area;
}

bool surface_is_focusable(surface_t *s) {
    return s->focusable;
}

bool surface_is_visible(surface_t *s) {
    return s->is_visible;
}

bool surface_is_opaque(surface_t *s) {
    return s->is_opaque;
}

bool surface_needs_redraw(surface_t *s) {
    return s->needs_redraw;
}

bool surface_accepts_mouse(surface_t *s) {
    return s->accepts_mouse;
}

void surface_mark_clean(surface_t *s) {
    s->dirty_area = area_zero();
    s->needs_redraw = false;
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

void surface_invalidate_area(surface_t *s, area a) {
    if (area_is_empty(a)) return;

    // damage area is local to surface, extend current area
    s->dirty_area = area_union(s->dirty_area, a); 
    s->needs_redraw = true;
}

void surface_invalidate_all(surface_t *s) {
    s->dirty_area = s->frame;
    s->needs_redraw = true;
}

void surface_add_view(surface_t *s, view_t *v) {
    view_add_child_view(s->root_view, v);
}

void surface_set_focused_view(surface_t *s, view_t *v) {
    if (s->focused_view == v)
        return;
    
    if (s->focused_view != NULL) {
        s->focused_view->focused = false;
        if (s->focused_view->callbacks.on_focus_lost)
            s->focused_view->callbacks.on_focus_lost(s->focused_view);
        view_invalidate(s->focused_view);
        s->focused_view = NULL;
    }

    s->focused_view = v;
    if (v != NULL) {
        v->focused = true;
        view_invalidate(v);
        if (v->callbacks.on_focus_gained)
            v->callbacks.on_focus_gained(v);
    }
}

// ------------------------------------------------------------

static void _surface_mark_area_dirty(void *owner_data, area dirty) {
    surface_t *s = (surface_t *)owner_data;
    surface_invalidate_area(s, dirty);

    if (s->owner_interface && s->owner_interface->surface_invalidated)
        s->owner_interface->surface_invalidated(s, area_to_global(dirty, s->frame));
}

static void _surface_request_focus(void *owner_data, view_t *v) {
    surface_t *s = (surface_t *)owner_data;
    surface_set_focused_view(s, v);
}

static void _surface_release_focus(void *owner_data, view_t *v) {
    surface_t *s = (surface_t *)owner_data;
    surface_set_focused_view(s, NULL);
}

// ------------------------------------------------------------

void surface_set_on_paint_behavior(surface_t *s, surface_paint_func *behavior) {
    s->callbacks.paint = behavior;
}

void surface_on_paint(surface_t *s, graphics_context_t *gc, area dirty) {
    // custom behavior
    if (s->callbacks.paint) {
        s->callbacks.paint(s, gc, dirty);
        return;
    }
    
    // default behavior
    if (s->root_view != NULL)
        s->root_view->callbacks.paint(s->root_view, gc, dirty);
}

void surface_on_key_event(surface_t *s, key_event_t e) {
    // custom behavior
    if (s->callbacks.on_key_event != NULL) {
        s->callbacks.on_key_event(s, e);
        return;
    }

    // default behavior
    // first, surface wide keys
    if (e.keycode == KEY_TAB && e.keymods == 0) {
        view_t *next = view_find_next_focusable(s->root_view, s->focused_view);
        surface_set_focused_view(s, next); // even if NULL
        return;
    }

    if (s->focused_view)
        s->focused_view->callbacks.on_key_event(s->focused_view, e);
}

void surface_on_mouse_event(surface_t *s, mouse_event_t e) {
    // custom behavior
    if (s->callbacks.on_mouse_event != NULL) {
        s->callbacks.on_mouse_event(s, e);
        return;
    }

    // default behavior
    view_t *hit_view = view_hit_test(s->root_view, e.pos);
    if (hit_view == NULL)
        return;
    
    if (hit_view->focusable)
        surface_set_focused_view(s, hit_view);
    
    if (hit_view->callbacks.on_mouse_event)
        hit_view->callbacks.on_mouse_event(hit_view, mouse_event_localized(e, hit_view->frame));
}

void surface_on_focus_gained(surface_t *s) {
    // custom behavior
    if (s->callbacks.on_focus_gained != NULL) {
        s->callbacks.on_focus_gained(s);
        return;
    }

    //default behavior
    if (s->focused_view == NULL)  // set initial focus view
        s->focused_view = view_find_first_focusable(s->root_view);

    if (s->focused_view)
        s->focused_view->callbacks.on_focus_gained(s->focused_view);
}

void surface_on_focus_lost(surface_t *s) {
    // custom behavior
    if (s->callbacks.on_focus_lost != NULL) {
        s->callbacks.on_focus_lost(s);
        return;
    }

    // default behavior
    if (s->focused_view)
        s->focused_view->callbacks.on_focus_lost(s->focused_view);
}

void surface_on_shown(surface_t *s) {
    if (s->callbacks.on_shown)
        s->callbacks.on_shown(s);
}

void surface_on_hidden(surface_t *s) {
    if (s->callbacks.on_hidden)
        s->callbacks.on_hidden(s);
}
