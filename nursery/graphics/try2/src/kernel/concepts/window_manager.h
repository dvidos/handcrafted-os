#pragma once
#include "events.h"
#include "window.h"


/*
    Main responsibilities:
    - own the window list
    - own / manipulate z-order
    - update screen manager accordingly
    - raise / lower windows
    - windows decorations (title, resize handles etc)
    - windows resizing / moving etc.
    - distributing events
*/

void initialize_window_manager();

void wm_register_window(window_t *w);
void wm_unregister_window(window_t *w);

// general operations
void wm_close_window(window_t *w);
void wm_show_window(window_t *w);
void wm_hide_window(window_t *w);

// reorders internal list, updates screen manager
void wm_raise_window(window_t *w);
void wm_lower_window(window_t *w);
window_t *wm_get_top_window();

// ensures one focused window
void wm_focus_window(window_t *w);
void wm_clear_focus();
window_t *wm_focused_window();

// hit test
window_t *wm_window_at_point(point screen_location);
typedef enum { WM_HIT_CLIENT, WM_HIT_TITLEBAR, WM_HIT_RESIZE_LEFT, WM_HIT_RESIZE_RIGHT, WM_HIT_CLOSE_BUTTON } wm_hit_t;
wm_hit_t wm_hit_test(window_t *w, point p); // titlebar, resize_left, close_button, client_area etc

void wm_dispatch_event(event_t *e);

// Determines target window, Handles move/resize initiation, Otherwise forwards to window
void wm_handle_mouse(mouse_event *e);

// Sent only to focused window
void wm_handle_key(key_event *e);

// tracks drag state, calls window_move/resize
void wm_begin_move(window_t *w, point start);
void wm_begin_resize(window_t *w, point start, uint32_t edges);
void wm_update_drag(point current);
void wm_end_drag(void);

// enumaration
window_t *wm_first_window(void);
window_t *wm_next_window(window_t *w);
