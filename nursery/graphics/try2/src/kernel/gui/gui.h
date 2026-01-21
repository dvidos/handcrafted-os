#pragma once
#include "../fundamentals.h"

typedef struct point {int x; int y;} point;
typedef struct size {int width; int height;} size;
typedef struct area {point pos; size size;} area;
typedef uint32_t color;
typedef struct gui_event gui_event;




// ----------- mouse manager ------------

typedef struct gui_mouse_manager_ops {
    point (*mouse_pos)();
    int (*set_mouse_pos)(point pos);
    int (*show_mouse)();
    int (*hide_mouse)();
    int (*set_mouse_pointer)(void *bitmap);
} gui_mouse_manager_ops;

// ----------- graphics context --------------

typedef struct gui_graphics_context_ops {
    void (*set_stroke_color)(gui_graphics_context *c, color color);
    void (*set_border_thickness)(gui_graphics_context *c, int thickness);

    void (*draw_filled_rectangle)(gui_graphics_context *c, area bounds);
    void (*draw_border)(gui_graphics_context *c, area bounds);
    void (*draw_text)(gui_graphics_context *c, const char *text);

    void (*destroy)(gui_graphics_context *c);
} gui_graphics_context_ops;

typedef struct gui_graphics_context {
    gui_graphics_context_ops *ops;
    void *private_data;
} gui_graphics_context;

// ----------- surfaces --------------

typedef struct gui_surface gui_surface;
typedef struct gui_surface_ops gui_surface_ops;

struct gui_surface {
    gui_surface_ops *ops;
    void *private_data;
};

typedef int (surface_handle_mouse_event_func)(gui_surface *s, gui_event e);
typedef int (surface_handle_key_event_func)(gui_surface *s, gui_event e);
typedef int (surface_paint_func)(gui_surface *s, gui_graphics_context *gc);
typedef int (surface_simple_event_func)(gui_surface *s);

struct gui_surface_ops {
    area (*get_frame)(gui_surface *s);
    void (*set_frame)(gui_surface *s, area frame);
    void (*paint)(gui_surface *s, area dirty_area);

    void (*set_mouse_event_handler)(gui_surface *s, surface_handle_mouse_event_func *handler);
    void (*set_keyboard_event_handler)(gui_surface *s, surface_handle_key_event_func *handler);
    void (*set_painter)(gui_surface *s, surface_paint_func *painter);
    void (*set_on_got_focus_handler)(gui_surface *s, surface_simple_event_func *handler);
    void (*set_on_lost_focus_handler)(gui_surface *s, surface_simple_event_func *handler);
    void (*set_on_shown_handler)(gui_surface *s, surface_simple_event_func *handler);
    void (*set_on_hidden_handler)(gui_surface *s, surface_simple_event_func *handler);

    bool (*handle_event)(gui_surface *s, gui_event e);
    void (*on_got_focus)(gui_surface *s);
    void (*on_lost_focus)(gui_surface *s);
    void (*on_shown)(gui_surface *s);
    void (*on_hidden)(gui_surface *s);

    void (*destroy)(gui_surface *s);
};

typedef struct gui_screen_manager_ops {
    size (*screen_size)();
    gui_surface (*create_surface)();
    int (*add_surface)(gui_surface *s);
    int (*remove_surface)(gui_surface *s);
    int (*raise_surface)(gui_surface *s);
    int (*sink_surface)(gui_surface *s);
    int (*mark_area_dirty)(area area);
    int (*redraw_dirty_areas)();
    int (*capture_mouse_events_to_surface)(gui_surface *s);
    int (*release_captured_mouse_events)();
    int (*handle_event)(gui_event e);
} gui_screen_manager_ops;

// ----------- windows ------------

typedef struct gui_window gui_window;
typedef struct gui_window_ops gui_window_ops;

struct gui_window {
    gui_window_ops *ops;
    void *private_data;
};

struct gui_window_ops {
    int (*handle_event)(gui_event e);
    void (*destroy)(gui_window *w);

    void (*set_client_data)(gui_window *w, void *data);
    void *(*get_client_data)(gui_window *w);
};

typedef struct gui_window_manager_ops {
    gui_window (*create_window)();
    int (*add_window)(gui_window *w);
    int (*remove_window)(gui_window *w);
    int (*raise_window)(gui_window *w);
    int (*sink_window)(gui_window *w);
    int (*maximize_window)(gui_window *w);
    int (*minimize_window)(gui_window *w);
    int (*make_window_full_screen)(gui_window *w);
    int (*restore_window)(gui_window *w);
    int (*handle_event)(gui_event e);
} gui_window_manager_ops;

// ----------- event management --------------

typedef struct gui_event_manager_ops {
    bool (*events_queued)();
    bool (*events_queue_empty)();
    void (*enqueue_event)(gui_event e);
    gui_event (*dequeue_event)();
} gui_event_manager_ops;

// ----------- gui entry point --------------

typedef struct gui_initialize_params {
    struct framebuffer {
        void *address;
        int width;
        int height;
        int pitch;
        int bpp;
    } framebuffer;
    u32 (*ticks)();
    void *(*malloc)(u32 size);
    void (*free)(void *ptr);
    void (*log)(const char *msg, ...);
} gui_initialize_params;

typedef struct gui_ops {
    // all the main functions supported by GUI
    // handle event, surface management, views, etc.
    // dependencies: framebuffer, heap, timer, events, logger, error codes, geometry

    int (*initialize)(gui_initialize_params *params);
    void (*handle_event)(gui_event *ev);
    void (*timer_ticked)();

    const gui_mouse_manager_ops mouse_manager;
    const gui_screen_manager_ops screen_manager;
    const gui_window_manager_ops window_manager;
    const gui_event_manager_ops event_manager;
} gui_ops;

extern gui_ops gui;

// ----------------------------------------------------------------------------

