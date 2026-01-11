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
    view_base_initialize(v, "base-view");
    return v;
}

void view_set_frame(view_t *v, area frame) {
    v->frame = frame;
    v->bounds = area_of(0, 0, frame.width, frame.height);
    view_invalidate(v);
}

void view_base_initialize(view_t *v, char *debug_info) {
    // set base properties...
    memset(v, 0, sizeof(view_t));

    // we could preset some useful presets here
    v->debug_info = debug_info;
    v->callbacks = view_default_callbacks;
    v->visible = true;
}

bool view_dispatch_mouse_event_deprecated(view_t *v, mouse_event_t e) {
    if (!v->visible)
        return false;

    // check children first
    for (view_t *child = v->children_head; child; child = child->next) {
        if (!area_contains(child->frame, e.pos))
            continue;

        if (view_dispatch_mouse_event_deprecated(child, mouse_event_localized(e, child->frame)))
            return true;
    }

    // if no child found or handled, we can handle
    return v->callbacks.on_mouse_event ? v->callbacks.on_mouse_event(v, e) : false;
}

void view_add_child_view(view_t *parent, view_t *child) {
    if (parent->children_tail == NULL) {
        // no children so far, add to both
        parent->children_head = child;
        parent->children_tail = child;
        child->next = NULL;
    } else {
        // add to tail, to maintain tab sequence
        parent->children_tail->next = child;
        parent->children_tail = child;
        child->next = NULL;
    }
    child->parent = parent; // needed to propagate 
}

void view_set_owner_interface(view_t *v, view_owner_interface_t *owner_interface, void *owner_data) {
    v->owner_interface = owner_interface;
    v->owner_data = owner_data;
}

void view_set_focused(view_t *v, bool focused) {
    if (focused == v->focused)
        return;
    if (focused && !v->focusable)
        return;

    v->focused = focused;
    view_invalidate(v);

    if (v->focused) {
        if (v->callbacks.on_focus_gained != NULL)
            v->callbacks.on_focus_gained(v);
    } else {
        if (v->callbacks.on_focus_lost != NULL)
            v->callbacks.on_focus_lost(v);
    }
}

void view_invalidate_area(view_t *v, area local_dirty) {
    // dirty area in local coords, bubbles up
    if (!v) return;

    if (v->parent) {
        // log.debug("view bubbling to parent");
        view_invalidate_area(v->parent, area_to_global(local_dirty, v->frame));
    }
    else if (v->owner_interface) {
        // log.debug("view bubbling to owner interface");
        v->owner_interface->mark_area_dirty(v->owner_data, area_to_global(local_dirty, v->frame));
    }
}

void view_invalidate(view_t *v) {
    if (!v) return;
    log.debug("view %s invalidated", v->debug_info);
    view_invalidate_area(v, v->bounds);
}

void view_paint_children(view_t *v, graphics_context_t *gc, area dirty) {
    for (view_t *child = v->children_head; child; child = child->next) {
        area child_dirty = area_to_local(dirty, child->frame);
        if (area_is_empty(child_dirty))
            continue;

        gc_push_state(gc);
        gc_move_origin(gc, child->frame.x, child->frame.y);
        child->callbacks.paint(child, gc, child_dirty);
        gc_pop_state(gc);
    }
}

view_t *view_hit_test(view_t *v, point p_local) {
    if (!point_is_inside(p_local, v->bounds))
        return NULL;

    for (view_t *child = v->children_head; child; child = child->next) {
        if (!child->visible)
            continue;
        
        point point_in_child = point_to_local(p_local, child->frame);
        view_t *hit = view_hit_test(child, point_in_child);
        if (hit)
            return hit;
    }

    return v;
}

static view_t *_next_focusable_after_current_recursively(view_t *root, view_t *current, bool *seen_current) {
    // returns the (recursively) first view after the current
    for (view_t *child = root->children_head; child; child = child->next) {
        
        if (*seen_current && child->focusable)
            return child;

        if (child == current)
            *seen_current = true;

        view_t *v = _next_focusable_after_current_recursively(child, current, seen_current);
        if (v)
            return v;
    }
    return NULL;
}

view_t *view_find_first_focusable(view_t *root) {
    view_t *target;
    for (view_t *child = root->children_head; child; child = child->next) {
        if (child->focusable)
            return child;

        target = view_find_first_focusable(child);
        if (target != NULL)
            return target;
    }

    return NULL;
}

view_t *view_find_next_focusable(view_t *root, view_t *focused) {
    bool seen_current = false;
    view_t *next = _next_focusable_after_current_recursively(root, focused, &seen_current);
    if (next)
        return next;
    
    // wrap around
    return view_find_first_focusable(root);
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
    // nothing by default
}

static void _base_view_on_focus_lost(view_t *v) {
    // nothing by default
}


static void _base_view_destroy(view_t *view) {

}

void view_log_debug_info(view_t *v, const char *prefix) {
    char buffer[128];

    log.info("%sview %-15s %p (%d,%d,%d,%d) visible=%c focusable=%c focused=%c parent=%p",
        prefix,
        v->debug_info,
        v,
        v->frame.x, v->frame.y, v->frame.width, v->frame.height, 
        v->visible ? 'y' : 'n',
        v->focusable ? 'y' : 'n',
        v->focused ? 'y' : 'n',
        v->parent
    );

    strcpy(buffer, prefix);
    strcat(buffer, "    ");
    for (view_t *child = v->children_head; child != NULL; child = child->next) {
        view_log_debug_info(child, buffer);
    }
}