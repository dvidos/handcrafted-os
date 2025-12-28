#pragma once
#include "../app_kit/view.h"
#include "../graphics/geometry.h"
#include "surface.h"

typedef struct window window_t;

struct window {
    // a window has views, z-order, flags, ...
    int z_order; // higher is up
    area frame;
    struct flags {
        uint16_t modal     :1;
        uint16_t closeable :1;
        uint16_t moveable  :1;
        uint16_t moving    :1;
        uint16_t resizable :1;
        uint16_t resizing  :1;
        uint16_t minimized :1;
        uint16_t maximized :1;
        uint16_t visible   :1;
        uint16_t focused   :1;
    } flags;

    // it should also own a screen surface, to notify the screen manager
    surface_t *surface;
    view_t *root_view;

    struct hooks {
        void (*on_closing)(window_t *w);
        void (*on_focus_changed)(window_t *w, int is_focused);
    } hooks;

    // wm ownership, doubly linked list
    window *next;
    window *prev;
};


window_t new_window(area frame);
void window_destroy(window_t *w);

// these add/remove the surface with the screen_manager and update the flag
void window_show(window_t *w);
void window_hide(window_t *w);

// geometry things, it updates frame, surface, marks areas dirty
void window_move(window_t *w, int x, int y);
void window_resize(window_t *w, int width, int height);
void window_set_frame(window_t *w, area frame);

// change z-order, win-list, call callbacks, etc
void window_raise(window_t *w);
void window_lower(window_t *w);
void window_focus(window_t *w);
void window_unfocus(window_t *w);

// events
bool window_hit_test(window_t *w, point l);
void window_dispatch_mouse(window_t *w, mouse_event *e);
void window_dispatch_key(window_t *w, key_event *e);

// other convenience ideas
area window_client_area(const window_t *w);
area window_titlebar_area(const window_t *w);
void window_mark_dirty(window_t *w, area area);
void window_redraw(window_t *w);
bool window_is_visible(const window_t *w);
bool window_is_focused(const window_t *w);
uint32_t window_flags(const window_t *w);
