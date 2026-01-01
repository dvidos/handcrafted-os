#include "../concepts/events.h"
#include "../graphics/geometry.h"
#include "view.h"

static void _paint(view_t *v, graphics_context_t *gc, area dirty) {
}

static bool _on_key_event(view_t *v, key_event_t e) {
}

static bool _on_mouse_event(view_t *v, mouse_event_t e) {
}

static void _on_focus_gained(view_t *v) {
    if (!v) return;
    if (v && v->focused) return;

    v->focused = true;
    v->callbacks->invalidate(v, v->bounds);
}

static void _on_focus_lost(view_t *v) {
    if (!v || !v->focused) return;

    v->focused = false;
    v->callbacks->invalidate(v, v->bounds);
}

static void _invalidate(view_t *v, area local_dirty) {
    // dirty area in local coords, bubbles up
    if (!v) return;

    if (v->parent) {
        view_invalidate(v->parent, area_to_global(local_dirty, v->frame));
    } else {
        // root view → surface ... nah, this is cyclic dependencies
        surface_t *s = view_get_surface(v);
        s->dirty_area = area_union(s->dirty_area, local_dirty);
    }
}

static void _destroy(view_t *view) {

}

bool view_dispatch_mouse(view_t *v, mouse_event_t e) {
    if (!v->visible)
        return false;

    // check children first
    for (view_t *child = v->children_list; child; child = child->list_next) {
        if (!area_contains(child->frame, e.pos))
            continue;

        if (view_dispatch_mouse(child, mouse_event_localized(e, child->frame)))
            return true;
    }

    // if no child found or handled, we can handle
    return v->callbacks->on_mouse_event ? v->callbacks->on_mouse_event(v, e) : false;
}

void view_base_initialize(view_t *v) {
    memset(v, 0, sizeof(view_t));
    // set base properties...

    v->callbacks->paint           = _paint;
    v->callbacks->on_key_event    = _on_key_event;
    v->callbacks->on_mouse_event  = _on_mouse_event;
    v->callbacks->on_focus_gained = _on_focus_gained;
    v->callbacks->on_focus_lost   = _on_focus_lost;
    v->callbacks->invalidate      = _invalidate;
    v->callbacks->destroy         = _destroy;

    // we could preset some useful presets here
    v->visible = true;
}

