#include "../concepts/logger.h"
#include "../concepts/events.h"
#include "../graphics/geometry.h"
#include "view.h"

static void _base_view_paint(view_t *v, graphics_context_t *gc, area dirty);
static bool _base_view_on_key_event(view_t *v, key_event_t e);
static bool _base_view_on_mouse_event(view_t *v, mouse_event_t e);
static void _base_view_on_focus_gained(view_t *v);
static void _base_view_on_focus_lost(view_t *v);
static void _base_view_destroy(view_t *view);

static view_callbacks_t view_default_callbacks = {
    .paint           = _base_view_paint,
    .on_key_event    = _base_view_on_key_event,
    .on_mouse_event  = _base_view_on_mouse_event,
    .on_focus_gained = _base_view_on_focus_gained,
    .on_focus_lost   = _base_view_on_focus_lost,
    .destroy         = _base_view_destroy,
};

// ----------------------------------------------------------------------

view_t *new_base_view() { // used by surfaces, for root view
    view_t *v = kmalloc(sizeof(view_t));
    view_base_initialize(v);
    return v;
}

void view_set_frame(view_t *v, area frame) {
    v->frame = frame;
    v->bounds = area_of(0, 0, frame.width, frame.height);
    view_mark_all_dirty(v);
}

void view_base_initialize(view_t *v) {
    // set base properties...
    memset(v, 0, sizeof(view_t));

    // we could preset some useful presets here
    v->callbacks = view_default_callbacks;
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
    return v->callbacks.on_mouse_event ? v->callbacks.on_mouse_event(v, e) : false;
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

    if (v->parent) {
        view_mark_area_dirty(v->parent, area_to_global(local_dirty, v->frame));
        log.debug("view bubbling to parent");
    }
    else if (v->owner_interface) {
        v->owner_interface->mark_area_dirty(v->owner_data, area_to_global(local_dirty, v->frame));
        log.debug("view bubbling to owner interface");
    }
}

void view_mark_all_dirty(view_t *v) {
    if (!v) return;
    view_mark_area_dirty(v, v->bounds);
}

void view_paint_children(view_t *v, graphics_context_t *gc, area dirty) {
    for (view_t *child = v->children_list; child; child = child->list_next) {
        area child_dirty = area_to_local(dirty, child->frame);
        if (area_is_empty(child_dirty))
            continue;

        gc_push_state(gc);
        gc_move_origin(gc, child->frame.x, child->frame.y);
        child->callbacks.paint(child, gc, child_dirty);
        gc_pop_state(gc);
    }
}

// -------------------------------------------------------------

static void _base_view_paint(view_t *v, graphics_context_t *gc, area dirty) {
    LOG_TRACE();
    view_paint_children(v, gc, dirty);
}

static bool _base_view_on_key_event(view_t *v, key_event_t e) {
    
}

static bool _base_view_on_mouse_event(view_t *v, mouse_event_t e) {
    // hit test?
}

static void _base_view_on_focus_gained(view_t *v) {
    if (!v) return;
    if (v && v->focused) return;

    v->focused = true;
    view_mark_all_dirty(v);
}

static void _base_view_on_focus_lost(view_t *v) {
    if (!v || !v->focused) return;

    v->focused = false;
    view_mark_all_dirty(v);
}


static void _base_view_destroy(view_t *view) {

}
