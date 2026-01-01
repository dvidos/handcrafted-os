#include "../concepts/events.h"
#include "../graphics/geometry.h"
#include "view.h"

static void _paint(view_t *v, graphics_context_t *gc, area dirty) {
    // we could do a red "X" for fun and to detect non painting children
}

static bool _on_key_event(view_t *v, key_event_t e) {
    // nothing
}

static bool _on_mouse_event(view_t *v, mouse_event_t e) {
    // nothing
}

static void _on_focus_gained(view_t *v) {
    if (!v) return;
    if (v && v->focused) return;

    v->focused = true;
    view_mark_all_dirty(v);
}

static void _on_focus_lost(view_t *v) {
    if (!v || !v->focused) return;

    v->focused = false;
    view_mark_all_dirty(v);
}


static void _destroy(view_t *view) {

}

view_t *new_base_view() { // used by surfaces, for root view
    view_t *v = kmalloc(sizeof(view_t));
    view_base_initialize(v);
    return v;
}

void view_base_initialize(view_t *v) {
    memset(v, 0, sizeof(view_t));
    // set base properties...

    v->callbacks->paint           = _paint;
    v->callbacks->on_key_event    = _on_key_event;
    v->callbacks->on_mouse_event  = _on_mouse_event;
    v->callbacks->on_focus_gained = _on_focus_gained;
    v->callbacks->on_focus_lost   = _on_focus_lost;
    v->callbacks->destroy         = _destroy;

    // we could preset some useful presets here
    v->visible = true;
}

bool view_dispatch_mouse_event(view_t *v, mouse_event_t e) {
    if (!v->visible)
        return false;

    // check children first
    for (view_t *child = v->children_list; child; child = child->list_next) {
        if (!area_contains(child->frame, e.pos))
            continue;

        if (view_dispatch_mouse_event(child, mouse_event_localized(e, child->frame)))
            return true;
    }

    // if no child found or handled, we can handle
    return v->callbacks->on_mouse_event ? v->callbacks->on_mouse_event(v, e) : false;
}

void view_add_child_view(view_t *parent, view_t *child) {
    child->list_next = parent->children_list;
    parent->children_list = child;
    child->parent = parent;
}

void view_set_owner_interface(view_t *v, view_owner_interface_t *owner_interface, void *owner_data) {
    v->owner_interface = owner_interface;
    v->owner_data = owner_data;
}

void view_mark_area_dirty(view_t *v, area local_dirty) {
    // dirty area in local coords, bubbles up
    if (!v) return;

    if (v->parent)
        view_mark_area_dirty(v->parent, area_to_global(local_dirty, v->frame));
    else if (v->owner_interface)
        v->owner_interface->mark_area_dirty(v->owner_data, area_to_global(local_dirty, v->frame));
}

void view_mark_all_dirty(view_t *v) {
    if (!v) return;
    view_mark_area_dirty(v, v->bounds);
}
