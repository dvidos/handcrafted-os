#include "../concepts/events.h"
#include "../concepts/surface.h"
#include "../graphics/geometry.h"

// a view is the common behavior exported by all widgets.
// it can be a desktop, window, panel, button, label, list, icon etc
// it seems the desktop is the root view, owning the background, the dock/panel, all the windows.
// the dock/panel owns the clock, icons, etc
// then each window owns decorations and client area
// each view has z-order, paints itself on its surface area / graphics context, that internally own 
// relations seem to be: view → graphics_context → gbuffer ← surface → screen_manager
// we can have a common view class with a vtable pointer, and each specific view (e.g. button/textbox) will encapsulate that common view in its attributes.

typedef struct view view_t;
typedef struct view_vtable view_vtable_t;

struct view_vtable {
    void (*draw)(view_t *view);
    int (*hit_test)(view_t *view, ...);
    int (*handle_event)(view_t *view, event_t *event);
    void (*destroy)(view_t *view);
};

typedef struct view {
    area area;
    view_t *parent;
    view_t *children;
    view_t *sibling;
    view_vtable_t *vt;
} view_t;


// each discrete view struct has the base view embedded as the first attribute. 
// This allows us to pass the pointer around as if it was a base view.
//  button_view_t *new_button_view();
//  textbox_view_t *new_input_box_view();
//  text_view_t *new_text_view();
//  slider_view_t *new_slider_view();
//  scrolling_view_t *new_scrolling_view();
//  list_view_t *new_list_view();
//  list_item_view_t *new_list_item_view();
//  menu_view_t *new_menu_view();
//  menu_item_view_t *new_menu_item_view();


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

    We can start with three:
    - Button_View
    - Text_View
    - Text_Box_View

*/
