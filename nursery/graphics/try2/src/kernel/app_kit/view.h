#pragma once
#include "../concepts/events.h"
#include "../graphics/geometry.h"
#include "graphics_context.h"
#include "ui_style.h" // all views shall need this...


// a view is the common behavior exported by all widgets.
// it can be a desktop, window, panel, button, label, list, icon etc
// it seems the desktop is the root view, owning the background, the dock/panel, all the windows.
// the dock/panel owns the clock, icons, etc
// then each window owns decorations and client area
// each view has z-order, paints itself on its surface area / graphics context, that internally own 
// relations seem to be: view → graphics_context → gbuffer ← surface → screen_manager
// we can have a common view class with a vtable pointer, and each specific view (e.g. button/textbox) will encapsulate that common view in its attributes.

/*
    NextStep's App kit NSView objects:
    - View (base class)
    - Window (not a view, but owns a view hierarchy)
    - Button
    - TextField
    - Slider
    - Matrix (grid of controls)
    - Form (??)
    - Text
    - TextView
    - Scroller
    - Box (containers)
    - ClipView
    - ScrollView
    - MenuView
    - MenuItem
    - ImageView
    - Browser (column browser, for file manager)
    - TableView
    - OutlineView (tree)
*/


typedef struct view view_t;
typedef struct view_vtable view_callbacks_t;

struct view_vtable {
    void (*paint)(view_t *v, graphics_context_t *gc, area dirty);
    bool (*on_key_event)(view_t *v, key_event_t e);
    bool (*on_mouse_event)(view_t *v, mouse_event_t e);
    void (*on_focus_gained)(view_t *v);
    void (*on_focus_lost)(view_t *v);
    void (*destroy)(view_t *view);
};

typedef struct view_owner_interface {
    void (*mark_area_dirty)(void *owner_data, area dirty);
    void (*request_focus)(void *owner_data, view_t *v);
    void (*release_focus)(void *owner_data, view_t *v); 
} view_owner_interface_t;

typedef struct view {
    area frame;   // relative to parent, for translating and hit testing
    area bounds;  // relative to zero (0,0,w,h) for dirty tracking, alignment, borders etc
    bool visible;
    bool focusable;
    bool focused;

    view_t *parent;
    view_t *children_list;
    view_t *list_next;
    view_callbacks_t callbacks;
    
    view_owner_interface_t *owner_interface; // allows bubbling up events without depending on surfaces.
    void *owner_data;
} view_t;

// to be used by child views
view_t *new_base_view(); // used by surfaces
void view_set_frame(view_t *v, area frame);

void view_base_initialize(view_t *v);
bool view_dispatch_mouse_event(view_t *v, mouse_event_t e);
void view_add_child_view(view_t *parent, view_t *child);
void view_set_owner_interface(view_t *v, view_owner_interface_t *owner, void *owner_data);
void view_mark_area_dirty(view_t *v, area local_dirty);
void view_mark_all_dirty(view_t *v);
void view_paint_children(view_t *v, graphics_context_t *gc, area dirty);
